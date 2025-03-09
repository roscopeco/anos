
/*
 * stage3 - IPC message channel
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdatomic.h>
#include <stdint.h>

#include "anos_assert.h"
#include "kdrivers/cpu.h"
#include "ktypes.h"
#include "once.h"
#include "panic.h"
#include "sched.h"
#include "slab/alloc.h"
#include "structs/hash.h"
#include "structs/list.h"
#include "task.h"

#ifdef CONSERVATIVE_BUILD
#include "kprintf.h"
#else
#ifdef DEBUG_CHANNEL_IPC
#include "kprintf.h"
#endif
#endif

#define INITIAL_CHANNEL_HASH_PAGE_COUNT ((4))
#define INITIAL_IN_FLIGHT_MESSAGE_HASH_PAGE_COUNT ((1))

typedef struct {
    ListNode this;
    uint64_t cookie;
    uint64_t tag;
    uint64_t arg;
    Task *waiter;
    uint64_t reply;
    bool handled;
} IpcMessage;

typedef struct {
    uint64_t cookie;
    Task *receivers;
    SpinLock *receivers_lock;
    IpcMessage *queue;
    SpinLock *queue_lock;
    uint64_t reserved[3];
} IpcChannel;

static_assert_sizeof(IpcMessage, ==, 64);
static_assert_sizeof(IpcChannel, ==, 64);

static _Atomic uint64_t next_channel_cookie;
static _Atomic uint64_t next_message_cookie;

static HashTable *channel_hash;
static HashTable *in_flight_message_hash;

void ipc_channel_init(void) {
    kernel_guard_once();

    next_channel_cookie = cpu_read_tsc();
    channel_hash = hash_table_create(INITIAL_CHANNEL_HASH_PAGE_COUNT);

    next_message_cookie = cpu_read_tsc();
    in_flight_message_hash =
            hash_table_create(INITIAL_IN_FLIGHT_MESSAGE_HASH_PAGE_COUNT);

    if (!channel_hash) {
        panic("Failed to initialise IPC channel hash");
    }
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

    uint64_t cookie =
            next_channel_cookie++; // just do ++ in case another thread cuts in...
    next_channel_cookie +=
            (cpu_read_tsc() & 0xffff); // ... then adjust by "random" value

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

uint64_t ipc_channel_recv(uint64_t cookie, uint64_t *tag, uint64_t *arg) {
    IpcChannel *channel = hash_table_lookup(channel_hash, cookie);

    if (channel) {
        spinlock_lock(channel->receivers_lock);

        // Check if there's a message already waiting...
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

            if (arg) {
                *arg = msg->arg;
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
            spinlock_unlock(channel->queue_lock);
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

            if (arg) {
                *arg = msg->arg;
            }

            return msg->cookie;
        }

        spinlock_unlock(channel->queue_lock);
        sched_unlock_this_cpu(lock_flags);
    }

    return 0;
}

static void init_message(IpcMessage *message, uint64_t tag, uint64_t arg,
                         Task *current_task) {

    uint64_t cookie =
            next_message_cookie++; // do ++ in case another thread cuts in...
    next_message_cookie +=
            (cpu_read_tsc() & 0xffff); // ... then adjust by "random" value

    message->this.next = 0;
    message->this.type = KTYPE_IPC_MESSAGE;
    message->cookie = cookie;
    message->tag = tag;
    message->arg = arg;
    message->waiter = current_task;
    message->reply = 0;
    message->handled = false;
}

uint64_t ipc_channel_send(uint64_t channel_cookie, uint64_t tag, uint64_t arg) {
    IpcChannel *channel = hash_table_lookup(channel_hash, channel_cookie);

    if (channel) {
        IpcMessage *message = slab_alloc_block();
        Task *current_task = task_current();

        if (!message) {
            return 0;
        }

        init_message(message, tag, arg, current_task);

        spinlock_lock(channel->queue_lock);

        if (channel->queue) {
            list_add((ListNode *)channel->queue, (ListNode *)message);
        } else {
            channel->queue = message;
            message->this.next = NULL;
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
