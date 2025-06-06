/*
 * stage3 - IPC message channel
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "anos_assert.h"
#include "capabilities/cookies.h"
#include "once.h"
#include "panic.h"
#include "sched.h"
#include "slab/alloc.h"
#include "std/string.h"
#include "structs/hash.h"
#include "structs/list.h"
#include "task.h"
#include "vmm/vmmapper.h"

#include "ipc/channel_internal.h"

#ifdef CONSERVATIVE_BUILD
#include "kprintf.h"
#else
#ifdef DEBUG_CHANNEL_IPC
#include "kprintf.h"
#endif
#endif

#define INITIAL_CHANNEL_HASH_PAGE_COUNT ((4))
#define INITIAL_IN_FLIGHT_MESSAGE_HASH_PAGE_COUNT ((1))
#define ARG_BUF_MAX ((0x1000))

#ifdef UNIT_TESTS
#define STATIC_EXCEPT_TESTS
#include "mock_pagetables.h"
#else
#define STATIC_EXCEPT_TESTS static
#endif

STATIC_EXCEPT_TESTS HashTable *channel_hash;
STATIC_EXCEPT_TESTS HashTable *in_flight_message_hash;

void ipc_channel_init(void) {
    kernel_guard_once();

    channel_hash = hash_table_create(INITIAL_CHANNEL_HASH_PAGE_COUNT);

    in_flight_message_hash =
            hash_table_create(INITIAL_IN_FLIGHT_MESSAGE_HASH_PAGE_COUNT);

    if (!channel_hash) {
        panic("Failed to initialise IPC channel hash");
    }
}

bool ipc_channel_exists(uint64_t cookie) {
    return hash_table_lookup(channel_hash, cookie) != NULL;
}

uint64_t ipc_channel_create(void) {
    IpcChannel *channel = slab_alloc_block();
    if (!channel) {
#ifdef CONSERVATIVE_BUILD
        kprintf("WARN: Failed to alloc for new channel\n");
#endif
        return 0;
    }

    channel->receivers_lock = slab_alloc_block();
    if (!channel->receivers_lock) {
#ifdef CONSERVATIVE_BUILD
        kprintf("WARN: Failed to alloc receivers lock for new channel\n");
#endif
        slab_free(channel);
        return 0;
    }

    channel->queue_lock = slab_alloc_block();
    if (!channel->queue_lock) {
#ifdef CONSERVATIVE_BUILD
        kprintf("WARN: Failed to alloc queue lock for new channel\n");
#endif
        slab_free(channel->receivers_lock);
        slab_free(channel);
        return 0;
    }

    spinlock_init(channel->receivers_lock);
    spinlock_init(channel->queue_lock);

    const uint64_t cookie = capability_cookie_generate();

    channel->cookie = cookie;
    channel->queue = NULL;
    channel->receivers = NULL;

    hash_table_insert(channel_hash, cookie, channel);

    return cookie;
}

void ipc_channel_destroy(uint64_t cookie) {
    // I know this _feels_ like it needs a lock, but the hash locks,
    // so this remove is atomic from the POV of users of the channel...
    //
    IpcChannel *channel = hash_table_remove(channel_hash, cookie);

    if (channel) {
        // First wake all senders who're blocked on messages. The
        // woken threads will know their send was never handled,
        // because the "handled" flag will not be set.
        //
        // We'll unblock them on any CPU, because we could potentially
        // be unblocking multiple at once here and don't want them all
        // fighting for this CPU...
        //
        // But that does mean they won't get a chance at scheduling
        // until the next sched on their target CPUs - because we
        // don't yet have a mechanism in place to signal other CPUs
        // they should run a schedule ASAP...
        //
        // TODO don't forget to come back and fix this comment when
        // we **do** have such a mechanism in place :D
        //
        IpcMessage *queued = channel->queue;
        while (queued) {
            if (!queued->waiter) {
                continue;
            }

            PerCPUState *target_cpu = sched_find_target_cpu();
            uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
            sched_unblock_on(queued->waiter, target_cpu);
            sched_unlock_any_cpu(target_cpu, lock_flags);
            queued = (IpcMessage *)queued->this.next;
        }

        // Now, same deal for receivers, they'll need a wakeup
        // too. These will know they were woken for this reason,
        // rather than because there's a message, because the first
        // thing they do on wake is check if the channel still exists...
        //
        Task *blocked_receiver = channel->receivers;
        while (blocked_receiver) {
            PerCPUState *target_cpu = sched_find_target_cpu();
            uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
            sched_unblock_on(blocked_receiver, target_cpu);
            sched_unlock_any_cpu(target_cpu, lock_flags);
            blocked_receiver = (Task *)blocked_receiver->this.next;
        }

        // Okay, we're done, we should be good to free the channel now.
        slab_free(channel->receivers_lock);
        slab_free(channel->queue_lock);
        slab_free(channel);

#ifdef CONSERVATIVE_BUILD
    } else {
        kprintf("WARN: Failed to find channel 0x%016lx for destroy\n", cookie);
#endif
    }
}

static inline unsigned int round_up_to_page_size(size_t size) {
    return (size + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
}

uint64_t ipc_channel_recv(uint64_t cookie, uint64_t *tag, size_t *buffer_size,
                          void *buffer) {
    if ((uintptr_t)buffer & PAGE_RELATIVE_MASK) {
        // buffer must be page aligned
        return 0;
    }

    IpcChannel *channel = hash_table_lookup(channel_hash, cookie);

    if (channel) {
        spinlock_lock(channel->receivers_lock);

        // Check if there's a message already waiting...
        //
        // TODO this could (will) allow a lower-priority receiver to
        // "jump the queue", need to figure out a way to fix that...
        //
        // (And ideally one that doesn't involve just queueing all receivers
        // and ignoring this check...)
        //
        spinlock_lock(channel->queue_lock);

        if (channel->queue) {
            IpcMessage *msg = channel->queue;
            channel->queue = (IpcMessage *)msg->this.next;
            hash_table_insert(in_flight_message_hash, msg->cookie, msg);
            spinlock_unlock(channel->queue_lock);
            spinlock_unlock(channel->receivers_lock);

            if (tag) {
                *tag = msg->tag;
            }

            if (buffer && msg->arg_buf_phys && msg->arg_buf_size) {
                vmm_map_page((uintptr_t)buffer, (uint64_t)msg->arg_buf_phys,
                             PG_USER | PG_READ | PG_WRITE | PG_PRESENT);
            } else {
                msg->arg_buf_phys = 0;
            }

            if (buffer_size) {
                *buffer_size = msg->arg_buf_size;
            }

            return msg->cookie;
        }

        spinlock_unlock(channel->queue_lock);

        Task *current_task = task_current();

        if (channel->receivers) {
            list_add((ListNode *)channel->receivers, (ListNode *)current_task);
        } else {
            channel->receivers = task_current();
            channel->receivers->this.next = NULL;
        }

        spinlock_unlock(channel->receivers_lock);

#ifdef DEBUG_CHANNEL_IPC
        kprintf("Locking task 0x%016lx\n", (uintptr_t)current_task);
#endif
        uint64_t lock_flags = sched_lock_this_cpu();
        sched_block(current_task);
        sched_schedule();

        // BLOCK HERE...
        // if this channel was destroyed, all waiting receivers will have been unblocked
        // **after the channel was removed from the hash**. So, before we continue
        // we'll fetch the channel again from the hash - if it was destroyed, we won't
        // find the channel...
        spinlock_lock(channel->queue_lock);

#ifdef DEBUG_CHANNEL_IPC
        kprintf("Unlocked task 0x%016lx\n", (uintptr_t)current_task);
#endif
        channel = hash_table_lookup(channel_hash, cookie);

        if (!channel) {
            sched_unlock_this_cpu(lock_flags);
            return 0;
        }

        if (channel->queue) {
            IpcMessage *msg = channel->queue;

            // Make sure we set the handled flag, see comments in ipc_channel_destroy
            // for an explanation as to what this is for...
            msg->handled = true;

            channel->queue = (IpcMessage *)msg->this.next;

            hash_table_insert(in_flight_message_hash, msg->cookie, msg);
            spinlock_unlock(channel->queue_lock);
            sched_unlock_this_cpu(lock_flags);

            if (tag) {
                *tag = msg->tag;
            }

            if (buffer && msg->arg_buf_phys && msg->arg_buf_size) {
                vmm_map_page((uintptr_t)buffer, msg->arg_buf_phys,
                             PG_USER | PG_READ | PG_WRITE | PG_PRESENT);
            } else {
                msg->arg_buf_phys = 0;
            }

            if (buffer_size) {
                *buffer_size = msg->arg_buf_size;
            }

            return msg->cookie;
        }

        spinlock_unlock(channel->queue_lock);
        sched_unlock_this_cpu(lock_flags);
    }

    return 0;
}

static bool init_message(IpcMessage *message, uint64_t tag, size_t size,
                         void *buffer, Task *current_task) {

    if (size > VM_PAGE_SIZE) {
        size = VM_PAGE_SIZE;
    }

    uint64_t cookie = capability_cookie_generate();

    message->this.next = 0;
    message->tag = tag;
    message->cookie = cookie;
    message->arg_buf_size = size;
    message->arg_buf_phys = vmm_virt_to_phys_page((uintptr_t)buffer);
    message->waiter = current_task;
    message->reply = 0;
    message->handled = false;

    return true;
}

uint64_t ipc_channel_send(uint64_t channel_cookie, uint64_t tag, size_t size,
                          void *buffer) {
    if ((uintptr_t)buffer & PAGE_RELATIVE_MASK) {
        // buffer must be page aligned
        return 0;
    }

    if (size > VM_PAGE_SIZE) {
        // Buffer can only be one page for now
        return 0;
    }

    IpcChannel *channel = hash_table_lookup(channel_hash, channel_cookie);

    if (channel) {
        IpcMessage *message = slab_alloc_block();
        Task *current_task = task_current();

        if (!message) {
            return 0;
        }

        if (!init_message(message, tag, size, buffer, current_task)) {
            return 0;
        }

        spinlock_lock(channel->queue_lock);

        if (channel->queue) {
            list_add((ListNode *)channel->queue, (ListNode *)message);
        } else {
            channel->queue = message;
        }

        spinlock_unlock(channel->queue_lock);

        spinlock_lock(channel->receivers_lock);
        uint64_t lock_flags = sched_lock_this_cpu();
        if (channel->receivers) {
            // unblock first receiver
            Task *receiver = channel->receivers;
            channel->receivers = (Task *)receiver->this.next;

            // Do this now so we're not holding it while we requeue the receiver...
            spinlock_unlock(channel->receivers_lock);
            sched_unblock(receiver);
        } else {
            // ... or just do this (in else, since we did it in the branch above too)
            spinlock_unlock(channel->receivers_lock);
        }

        sched_block(current_task);
        sched_schedule();
        sched_unlock_this_cpu(lock_flags);

        uint64_t result = message->reply;

#ifdef CONSERVATIVE_BUILD
        // This should never happen (it should've been dequeued while we were sleeping)
        // but just in case of weirdness...
        //
        // Though note, we **do** expect this to happen in the unit tests, since the mock
        // scheduler doesn't actually block!
        if (channel->queue == message) {
            kprintf("WARN: Queued message not dequeued by the time send "
                    "completed...\n");
            channel->queue = (IpcMessage *)message->this.next;
        }
#endif

        if (message->arg_buf_phys) {
            // TODO Not sure why I'm doing this, I suspect it's a bug.
            // why would the phys be mapped virtual?
            //
            // I _think_ I'm wanting to unmap in the _receiving_ process,
            // which means this would need to (somehow) be done in reply, below...
            vmm_unmap_page(message->arg_buf_phys);
        }
        slab_free(message);
        return result;
    }

    return 0;
}

uint64_t ipc_channel_reply(uint64_t message_cookie, uint64_t result) {
    IpcMessage *msg = hash_table_lookup(in_flight_message_hash, message_cookie);

    if (!msg) {
        return 0;
    }

    hash_table_remove(in_flight_message_hash, message_cookie);

    msg->reply = result;

    uint64_t lock_flags = sched_lock_this_cpu();
    sched_unblock(msg->waiter);
    sched_schedule();
    sched_unlock_this_cpu(lock_flags);

    return message_cookie;
}
