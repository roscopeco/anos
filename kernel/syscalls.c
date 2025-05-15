/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 * 
 * Copyright (c) 2024 Ross Bamford
 * 
 * NOTE: The layout of calls defined herein is currently only 
 * for test/development. They are poorly designed and **won't**
 * be sticking around in this layout. Some of the calls here
 * are only for debugging and will also be removed!
 * 
 * TODO these **badly** need their return values sorting out, in some
 * error cases callers have no way to know what they're going to get
 * (even in terms of signedness!)
 */

#include <stdint.h>

#include "capabilities/cookies.h"
#include "capabilities/map.h"
#include "debugprint.h"
#include "ipc/channel.h"
#include "ipc/named.h"
#include "kprintf.h"
#include "pmm/pagealloc.h"
#include "process.h"
#include "process/address_space.h"
#include "process/memory.h"
#include "sched.h"
#include "slab/alloc.h"
#include "sleep.h"
#include "smp/state.h"
#include "std/string.h"
#include "structs/region_tree.h"
#include "syscalls.h"
#include "task.h"
#include "throttle.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_PROCESS_SYSCALLS
#include "printhex.h"
#endif

#ifdef CONSERVATIVE_BUILD
#include "panic.h"
#endif

#define MIN_PROCESS_STACK_HEADROOM ((1024))
#define STACK_VALUE_SIZE ((sizeof(uintptr_t)))

extern CapabilityMap global_capability_map;

typedef void (*ThreadFunc)(void);

extern MemoryRegion *physical_region;

#define SYSCALL_ARGS                                                           \
    SyscallArg arg0, SyscallArg arg1, SyscallArg arg2, SyscallArg arg3,        \
            SyscallArg arg4
#define SYSCALL_NAME(name) handle_##name
#define SYSCALL_HANDLER(name)                                                  \
    static SyscallResult SYSCALL_NAME(name)(SYSCALL_ARGS)

SYSCALL_HANDLER(debugprint) {
    char *message = (char *)arg0;

    if (IS_USER_ADDRESS(message)) {
        kprintf("%s", message);
    }

    return SYSCALL_OK;
}

SYSCALL_HANDLER(debugchar) {
    const char chr = (char)arg0;

    debugchar(chr);

    return SYSCALL_OK;
}

SYSCALL_HANDLER(create_thread) {
    const ThreadFunc func = (ThreadFunc)arg0;
    const uintptr_t user_stack = (uintptr_t)arg1;

    Task *task = task_create_user(task_current()->owner, user_stack, 0,
                                  (uintptr_t)func, TASK_CLASS_NORMAL);

#ifdef SYSCALL_SCHED_ONLY_THIS_CPU
    sched_lock_this_cpu();
    sched_unblock(task);
    sched_unlock_this_cpu();
#else
    PerCPUState *target_cpu = sched_find_target_cpu();
    const uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return task->sched->tid;
}

SYSCALL_HANDLER(memstats) {
    AnosMemInfo *mem_info = (AnosMemInfo *)arg0;

    if (IS_USER_ADDRESS(mem_info)) {
        mem_info->physical_total = physical_region->size;
        mem_info->physical_avail = physical_region->free;
    }

    return SYSCALL_OK;
}

SYSCALL_HANDLER(sleep) {
    const uint64_t nanos = (uint64_t)arg0;

    const uint64_t lock_flags = sched_lock_this_cpu();
    sleep_task(task_current(), nanos);
    sched_unlock_this_cpu(lock_flags);

    return SYSCALL_OK;
}

SYSCALL_HANDLER(create_process) {
    ProcessCreateParams *process_create_params = (ProcessCreateParams *)arg0;

    // Validate process create is in userspace
    if (!IS_USER_ADDRESS(process_create_params)) {
        return SYSCALL_BADARGS;
    }

    // Validate stack arguments
    if (process_create_params->stack_base >= VM_KERNEL_SPACE_START ||
        (process_create_params->stack_base +
         process_create_params->stack_size) >= VM_KERNEL_SPACE_START) {
        return SYSCALL_BADARGS;
    }

    // Ensure number of requested stack values is valid
    if (process_create_params->stack_value_count > MAX_STACK_VALUE_COUNT) {
        return SYSCALL_BADARGS;
    }

    // Ensure enough space on stack for the requested values + minimum headroom
    if ((process_create_params->stack_value_count * STACK_VALUE_SIZE) >
        (process_create_params->stack_size - MIN_PROCESS_STACK_HEADROOM)) {
        return (int64_t)SYSCALL_BADARGS;
    }

    // Validate regions arguments
    if (process_create_params->region_count > MAX_PROCESS_REGIONS) {
        return SYSCALL_BADARGS;
    }

    AddressSpaceRegion ad_regions[process_create_params->region_count];

    for (int i = 0; i < process_create_params->region_count; i++) {
        ProcessMemoryRegion *src_ptr = &process_create_params->regions[i];
        AddressSpaceRegion *dst_ptr = &ad_regions[i];

        if (((uintptr_t)src_ptr) > VM_KERNEL_SPACE_START) {
            return SYSCALL_BADARGS;
        }

        if (((uintptr_t)src_ptr) + sizeof(AddressSpaceRegion) >
            VM_KERNEL_SPACE_START) {
            return SYSCALL_BADARGS;
        }

        // Region start and length must be page aligned
        if (src_ptr->start & 0xfff) {
            return SYSCALL_BADARGS;
        }

        if (src_ptr->len_bytes & 0xfff) {
            return SYSCALL_BADARGS;
        }

        dst_ptr->start = src_ptr->start;
        dst_ptr->len_bytes = src_ptr->len_bytes;
    }

    uintptr_t new_pml4 = address_space_create(
            process_create_params->stack_base,
            process_create_params->stack_size,
            process_create_params->region_count, ad_regions,
            process_create_params->stack_value_count,
            process_create_params->stack_values);

    if (!new_pml4) {
        debugstr("Failed to create address space\n");
        return SYSCALL_FAILURE;
    }

    Process *new_process = process_create(new_pml4);

    if (!new_process) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new process\n");
        return SYSCALL_FAILURE;
    }

    Task *new_task =
            task_create_user(new_process,
                             process_create_params->stack_base +
                                     process_create_params->stack_size -
                                     (process_create_params->stack_value_count *
                                      sizeof(uintptr_t)),
                             0, (uintptr_t)process_create_params->entry_point,
                             TASK_CLASS_NORMAL);

    if (!new_task) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new task\n");
        process_destroy(new_process);
        return SYSCALL_FAILURE;
    }

#ifdef DEBUG_PROCESS_SYSCALLS
    debugstr("Created new process ");
    printhex8(new_process->pid, debugchar);
    debugstr(" @ ");
    printhex64((uintptr_t)new_process, debugchar);
    debugstr(" with PML4 @ ");
    printhex64(new_pml4, debugchar);
    debugstr("\n");
#endif

#ifdef SYSCALL_SCHED_ONLY_THIS_CPU
    sched_lock_this_cpu();
    sched_unblock(task);
    sched_unlock_this_cpu();
#else
    PerCPUState *target_cpu = sched_find_target_cpu();
    const uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(new_task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return new_process->pid;
}

static inline uintptr_t page_align(uintptr_t addr) {
    return addr & 0xfff ? (addr + VM_PAGE_SIZE) & ~(0xfff) : addr;
}

static inline void undo_partial_map(const uintptr_t virtual_base,
                                    const uintptr_t virtual_last,
                                    const uintptr_t new_page) {
    if ((new_page & 0xff) == 0) {
        // we allocated a page, so must be failing because there's already a page
        // mapped here... We need to free the page we allocated.
        process_page_free(task_current()->owner, new_page);
    }

    // Either way, we're failing, so need to undo what we've already done...
    for (uintptr_t unmap_addr = virtual_base; unmap_addr < virtual_last;
         unmap_addr += VM_PAGE_SIZE) {
        uintptr_t page = vmm_virt_to_phys_page(unmap_addr);

#ifdef CONSERVATIVE_BUILD
        if (!page) {
            panic("unmapped page found during map_virtual syscall failure "
                  "loop");
        }
#endif
        vmm_unmap_page(unmap_addr);
        // TODO likely some opportunity to make this more
        //      efficient, it's spinning through the owned
        //      page list to free each individual page...
        process_page_free(task_current()->owner, page);
    }
}

SYSCALL_HANDLER(map_virtual) {
    size_t size = (size_t)arg0;
    uintptr_t virtual_base = (uintptr_t)arg1;
    uint64_t type = (uint64_t)arg2;
    uint64_t flags = (uint64_t)arg3;
    uint64_t arg = (uint64_t)arg4;

    if (size == 0) {
        // we don't map empty regions...
        return 0;
    }

    virtual_base = page_align(virtual_base);
    size = page_align(size);

    if (!IS_USER_ADDRESS(virtual_base)) {
        // nice try...
        return 0;
    }

    if (!IS_USER_ADDRESS(virtual_base + size)) {
        // also nope...
        return 0;
    }

    // Let's try to map it in, just using small pages for now...
    const uintptr_t virtual_end = virtual_base + size;
    for (uintptr_t addr = virtual_base; addr < virtual_end;
         addr += VM_PAGE_SIZE) {
        const uintptr_t new_page =
                process_page_alloc(task_current()->owner, physical_region);

        if (new_page & 0xff || vmm_virt_to_phys_page(addr)) {
            undo_partial_map(virtual_base, addr, new_page);
            return 0;
        }

        // TODO allow flags to be controlled (to an extent) by caller...
        if (!vmm_map_page(addr, new_page, PG_PRESENT | PG_WRITE | PG_USER)) {
            undo_partial_map(virtual_base, addr, new_page);
            return 0;
        }
    }

    memclr((void *)virtual_base, size);

    return virtual_base;
}

SYSCALL_HANDLER(unmap_virtual) {
    size_t size = (size_t)arg0;
    uintptr_t virtual_base = (uintptr_t)arg1;

    virtual_base = page_align(virtual_base);
    size = page_align(size);

    if (!IS_USER_ADDRESS(virtual_base)) {
        return SYSCALL_BADARGS;
    }

    if (!IS_USER_ADDRESS(virtual_base + size)) {
        return SYSCALL_BADARGS;
    }

    while (size > 0) {
        vmm_unmap_page(virtual_base);

        virtual_base += VM_PAGE_SIZE;
        size -= VM_PAGE_SIZE;
    }

    return SYSCALL_OK;
}

// TODO we need a nicer scheme for return values in all of the below...
// they're kind of all over the place at the moment...
//

SYSCALL_HANDLER(send_message) {
    const uint64_t channel_cookie = (uint64_t)arg0;
    const uint64_t tag = (uint64_t)arg1;
    const size_t size = (size_t)arg2;
    void *buffer = (void *)arg3;

    if (IS_USER_ADDRESS(buffer)) {
        return ipc_channel_send(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SYSCALL_HANDLER(recv_message) {
    const uint64_t channel_cookie = (uint64_t)arg0;
    uint64_t *tag = (uint64_t *)arg1;
    size_t *size = (size_t *)arg2;
    void *buffer = (void *)arg3;

    if (IS_USER_ADDRESS(tag) && IS_USER_ADDRESS(size) &&
        IS_USER_ADDRESS(buffer)) {
        return ipc_channel_recv(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SYSCALL_HANDLER(reply_message) {
    const uint64_t message_cookie = (uint64_t)arg0;
    const uint64_t reply = (uint64_t)arg1;

    return ipc_channel_reply(message_cookie, reply);
}

SYSCALL_HANDLER(create_channel) { return ipc_channel_create(); }

SYSCALL_HANDLER(destroy_channel) {
    const uint64_t cookie = (uint64_t)arg0;

    ipc_channel_destroy(cookie);
    return SYSCALL_OK;
}

SYSCALL_HANDLER(register_named_channel) {
    const uint64_t cookie = (uint64_t)arg0;
    char *name = (char *)arg1;

    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_register(cookie, name)) {
        return SYSCALL_OK;
    } else {
        return SYSCALL_BAD_NUMBER;
    }
}

SYSCALL_HANDLER(deregister_named_channel) {
    char *name = (char *)arg0;

    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_deregister(name) != 0) {
        return SYSCALL_OK;
    }

    return SYSCALL_BAD_NAME;
}

SYSCALL_HANDLER(find_named_channel) {
    char *name = (char *)arg0;

    if (!IS_USER_ADDRESS(name)) {
        return 0;
    }

    return named_channel_find(name);
}

void thread_exitpoint(void);

SYSCALL_HANDLER(kill_current_task) {
    const Task *current = task_current();

    if (current) {
        sched_lock_this_cpu();

        current->sched->status_flags |= TASK_SCHED_FLAG_KILLED;

        sched_schedule();

        __builtin_unreachable();
    } else {
        return 0;
    }
}

/*
 * NOTE:
 * This syscall layer rejects overlapping regions, but *allows* adjacent ones.
 * If a region is adjacent to an existing region with the same metadata, we
 * coalesce them into one. This prevents AVL tree bloat from tiny heaps or
 * allocator fragmentation.
 */

static Region *region_tree_find_adjacent(Region *node, const uintptr_t start,
                                         const uintptr_t end,
                                         const void *metadata) {
    while (node) {
        if (end == node->start && metadata == node->metadata) {
            return node;
        }
        if (start == node->end && metadata == node->metadata) {
            return node;
        }
        if (end <= node->start) {
            node = node->left;
        } else if (start >= node->end) {
            node = node->right;
        } else {
            break;
        }
    }
    return nullptr;
}

static bool region_tree_overlaps(const Region *node, const uintptr_t start,
                                 const uintptr_t end) {
    while (node) {
        if (start < node->end && end > node->start) {
            return true;
        }
        if (end <= node->start) {
            node = node->left;
        } else {
            node = node->right;
        }
    }
    return false;
}

SYSCALL_HANDLER(create_region) {
    const uintptr_t start = (uintptr_t)arg0;
    const uintptr_t end = (uintptr_t)arg1;
    void *metadata = (void *)arg2;

    if ((start & 0xFFF) || (end & 0xFFF) || end <= start ||
        start >= USERSPACE_LIMIT || end > USERSPACE_LIMIT) {
        return SYSCALL_BADARGS;
    }

    const Process *proc = task_current()->owner;

    // Try coalescing with an adjacent region if one exists
    Region *adj = region_tree_find_adjacent(proc->meminfo->regions, start, end,
                                            metadata);
    if (adj) {
        if (end == adj->start) {
            adj->start = start;
        } else if (start == adj->end) {
            adj->end = end;
        }
        return SYSCALL_OK;
    }

    if (region_tree_overlaps(proc->meminfo->regions, start, end)) {
        return SYSCALL_FAILURE; // overlap not allowed
    }

    Region *region = slab_alloc_block();
    if (!region) {
        return SYSCALL_FAILURE;
    }

    *region = (Region){
            .start = start,
            .end = end,
            .metadata = metadata,
            .left = nullptr,
            .right = nullptr,
            .height = 1,
    };

    proc->meminfo->regions = region_tree_insert(proc->meminfo->regions, region);
    return SYSCALL_OK;
}

SYSCALL_HANDLER(destroy_region) {
    const uintptr_t start = (uintptr_t)arg0;

    if ((start & 0xFFF) || start >= USERSPACE_LIMIT) {
        return SYSCALL_BADARGS;
    }

    const Process *proc = task_current()->owner;
    proc->meminfo->regions = region_tree_remove(proc->meminfo->regions, start);
    return SYSCALL_OK;
}

static uint64_t init_syscall_capability(CapabilityMap *map,
                                        SyscallId syscall_id,
                                        SyscallHandler handler) {
    if (!map) {
        return 0;
    }

    SyscallCapability *capability = slab_alloc_block();

    if (!capability) {
        return 0;
    }

    uint64_t cookie = capability_cookie_generate();

    if (!cookie) {
        slab_free(capability);
        return 0;
    }

    capability->this.type = CAPABILITY_TYPE_SYSCALL;
    capability->this.subtype = 1;
    capability->syscall_id = syscall_id;
    capability->flags = 0;
    capability->handler = handler;

    if (!capability_map_insert(map, cookie, capability)) {
        slab_free(capability);
        return 0;
    }

    return cookie;
}

#define STACKED_LONGS_PER_CAPABILITY ((2))

#define stack_syscall_capability_cookie(id, handler)                           \
    do {                                                                       \
        uint64_t cookie =                                                      \
                init_syscall_capability(&global_capability_map, id, handler);  \
        if (!cookie) {                                                         \
            return nullptr;                                                    \
        }                                                                      \
        *--current_stack = cookie;                                             \
        *--current_stack = id;                                                 \
    } while (0)

uint64_t *syscall_init_capabilities(uint64_t *stack) {
    uint64_t *current_stack = stack;

    // Stack all syscall capability cookies...
    stack_syscall_capability_cookie(SYSCALL_ID_DEBUG_PRINT,
                                    SYSCALL_NAME(debugprint));
    stack_syscall_capability_cookie(SYSCALL_ID_DEBUG_CHAR,
                                    SYSCALL_NAME(debugchar));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_THREAD,
                                    SYSCALL_NAME(create_thread));
    stack_syscall_capability_cookie(SYSCALL_ID_MEMSTATS,
                                    SYSCALL_NAME(memstats));
    stack_syscall_capability_cookie(SYSCALL_ID_SLEEP, SYSCALL_NAME(sleep));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_PROCESS,
                                    SYSCALL_NAME(create_process));
    stack_syscall_capability_cookie(SYSCALL_ID_MAP_VIRTUAL,
                                    SYSCALL_NAME(map_virtual));
    stack_syscall_capability_cookie(SYSCALL_ID_SEND_MESSAGE,
                                    SYSCALL_NAME(send_message));
    stack_syscall_capability_cookie(SYSCALL_ID_RECV_MESSAGE,
                                    SYSCALL_NAME(recv_message));
    stack_syscall_capability_cookie(SYSCALL_ID_REPLY_MESSAGE,
                                    SYSCALL_NAME(reply_message));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_CHANNEL,
                                    SYSCALL_NAME(create_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_DESTROY_CHANNEL,
                                    SYSCALL_NAME(destroy_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_REGISTER_NAMED_CHANNEL,
                                    SYSCALL_NAME(register_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_DEREGISTER_NAMED_CHANNEL,
                                    SYSCALL_NAME(deregister_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_FIND_NAMED_CHANNEL,
                                    SYSCALL_NAME(find_named_channel));
    stack_syscall_capability_cookie(SYSCALL_ID_KILL_CURRENT_TASK,
                                    SYSCALL_NAME(kill_current_task));
    stack_syscall_capability_cookie(SYSCALL_ID_UNMAP_VIRTUAL,
                                    SYSCALL_NAME(unmap_virtual));
    stack_syscall_capability_cookie(SYSCALL_ID_CREATE_REGION,
                                    SYSCALL_NAME(create_region));
    stack_syscall_capability_cookie(SYSCALL_ID_DESTROY_REGION,
                                    SYSCALL_NAME(destroy_region));

    // Stack dummy argc/argv for now
    // TODO this needs refactoring, syscall init shouldn't be responsible
    //      for this
    *--current_stack = 0;
    *--current_stack = 0;

    // Stack a pointer to the first cookie (at the top of the stack)
    --current_stack;
    *current_stack = (uint64_t)(current_stack + 3);

    // Stack the number of cookies
    *--current_stack = (SYSCALL_ID_END - 1);

    return current_stack;
}

SyscallResult handle_syscall_69(const SyscallArg arg0, const SyscallArg arg1,
                                const SyscallArg arg2, const SyscallArg arg3,
                                const SyscallArg arg4,
                                const SyscallArg capability_cookie) {

    Process *proc = task_current()->owner;

    const SyscallCapability *cap = capability_map_lookup(
            &global_capability_map, (uint64_t)capability_cookie);

    if (cap && cap->handler) {
        throttle_reset(proc);
        return cap->handler(arg0, arg1, arg2, arg3, arg4);
    }

    throttle_abuse(proc);
    return SYSCALL_INCAPABLE;
}
