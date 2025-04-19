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

#include "debugprint.h"
#include "fba/alloc.h"
#include "ipc/channel.h"
#include "ipc/named.h"
#include "kprintf.h"
#include "pmm/pagealloc.h"
#include "printhex.h"
#include "process.h"
#include "sched.h"
#include "sleep.h"
#include "smp/state.h"
#include "std/string.h"
#include "syscalls.h"
#include "task.h"
#include "vmm/vmconfig.h"
#include "vmm/vmmapper.h"
#include "x86_64/kdrivers/cpu.h"
#include "x86_64/process/address_space.h"
#include "x86_64/vmm/recursive.h"

#ifdef CONSERVATIVE_BUILD
#include "panic.h"
#endif

typedef void (*ThreadFunc)(void);

extern MemoryRegion *physical_region;

#ifdef DEBUG_TEST_SYSCALL
static SyscallResult handle_anos_testcall(SyscallArg arg0, SyscallArg arg1,
                                          SyscallArg arg2, SyscallArg arg3,
                                          SyscallArg arg4) {
    debugstr("TESTCALL: ");
    printhex8(arg0, debugchar);
    debugstr(" : ");
    printhex8(arg1, debugchar);
    debugstr(" : ");
    printhex8(arg2, debugchar);
    debugstr(" : ");
    printhex8(arg3, debugchar);
    debugstr(" : ");
    printhex8(arg4, debugchar);
    debugstr(" = ");

    return 42;
}
#endif

static SyscallResult handle_debugprint(char *message) {
    if (IS_USER_ADDRESS(message)) {
        kprintf("%s", message);
    }

    return SYSCALL_OK;
}

static SyscallResult handle_debugchar(char chr) {
    debugchar(chr);

    return SYSCALL_OK;
}

static SyscallResult handle_create_thread(ThreadFunc func,
                                          uintptr_t user_stack) {
    Task *task = task_create_user(task_current()->owner, user_stack, 0,
                                  (uintptr_t)func, TASK_CLASS_NORMAL);

#ifdef SYSCALL_SCHED_ONLY_THIS_CPU
    sched_lock_this_cpu();
    sched_unblock(task);
    sched_unlock_this_cpu();
#else
    PerCPUState *target_cpu = sched_find_target_cpu();
    uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return task->sched->tid;
}

static SyscallResult handle_memstats(AnosMemInfo *mem_info) {
    if (IS_USER_ADDRESS(mem_info)) {
        mem_info->physical_total = physical_region->size;
        mem_info->physical_avail = physical_region->free;
    }

    return SYSCALL_OK;
}

static SyscallResult handle_sleep(uint64_t nanos) {
    uint64_t lock_flags = sched_lock_this_cpu();
    sleep_task(task_current(), nanos);
    sched_unlock_this_cpu(lock_flags);

    return SYSCALL_OK;
}

static SyscallResult handle_create_process(uintptr_t stack_base,
                                           uint64_t stack_size,
                                           uint64_t region_count,
                                           ProcessMemoryRegion *regions,
                                           uintptr_t entry_point) {
    // Validate stack arguments
    if (stack_base >= VM_KERNEL_SPACE_START ||
        (stack_base + stack_size) >= VM_KERNEL_SPACE_START) {
        return SYSCALL_BADARGS;
    }

    // Validate regions argument
    if (region_count > MAX_PROCESS_REGIONS) {
        return SYSCALL_BADARGS;
    }

    AddressSpaceRegion ad_regions[region_count];

    for (int i = 0; i < region_count; i++) {
        ProcessMemoryRegion *src_ptr = &regions[i];
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

    uintptr_t new_stack = page_alloc(physical_region);

    if (!new_stack) {
        return SYSCALL_FAILURE;
    }

    uintptr_t new_pml4 = address_space_create(stack_base, stack_size,
                                              region_count, ad_regions);

    if (!new_pml4) {
        debugstr("Failed to create address space\n");
        page_free(physical_region, new_stack);
        return SYSCALL_FAILURE;
    }

    Process *new_process = process_create(new_pml4);

    if (!new_process) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new process\n");
        page_free(physical_region, new_stack);
        return SYSCALL_FAILURE;
    }

    Task *new_task = task_create_user(new_process, stack_base + stack_size, 0,
                                      entry_point, TASK_CLASS_NORMAL);

    if (!new_task) {
        // TODO LEAK address_space_destroy!
        debugstr("Failed to create new task\n");
        page_free(physical_region, new_stack);
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
    uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
    sched_unblock_on(new_task, target_cpu);
    sched_unlock_any_cpu(target_cpu, lock_flags);
#endif

    return new_process->pid;
}

static inline uintptr_t page_align(uintptr_t addr) {
    return addr & 0xfff ? (addr + VM_PAGE_SIZE) & ~(0xfff) : addr;
}

static inline void undo_partial_map(uintptr_t virtual_base,
                                    uintptr_t virtual_last,
                                    uintptr_t new_page) {
    if ((new_page & 0xff) == 0) {
        // we allocated a page, so must be failing because there's already a page
        // mapped here... We need to free the page we allocated.
        page_free(physical_region, new_page);
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
        page_free(physical_region, page);
    }
}

SyscallResult handle_map_virtual(uint64_t size, uintptr_t virtual_base) {
    if (size == 0) {
        // we don't map empty regions...
        return 0;
    }

    virtual_base = page_align(virtual_base);
    size = page_align(size);

    uint64_t page_count = size >> VM_PAGE_LINEAR_SHIFT;

    // Let's try to map it in, just using small pages for now...
    uintptr_t virtual_end = virtual_base + size;
    for (uintptr_t addr = virtual_base; addr < virtual_end;
         addr += VM_PAGE_SIZE) {
        uintptr_t new_page = page_alloc(physical_region);

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

// TODO we need a nicer scheme for return values in all of the below...
// they're kind of all over the place at the moment...
//

SyscallResult handle_send_message(uint64_t channel_cookie, uint64_t tag,
                                  size_t size, void *buffer) {
    if (IS_USER_ADDRESS(buffer)) {
        return ipc_channel_send(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SyscallResult handle_recv_message(uint64_t channel_cookie, uint64_t *tag,
                                  size_t *size, void *buffer) {

    if (IS_USER_ADDRESS(tag) && IS_USER_ADDRESS(size) &&
        IS_USER_ADDRESS(buffer)) {
        return ipc_channel_recv(channel_cookie, tag, size, buffer);
    }

    return 0;
}

SyscallResult handle_reply_message(uint64_t message_cookie, uint64_t reply) {
    return ipc_channel_reply(message_cookie, reply);
}

SyscallResult handle_create_channel(void) { return ipc_channel_create(); }

SyscallResult handle_destroy_channel(uint64_t cookie) {
    ipc_channel_destroy(cookie);
    return SYSCALL_OK;
}

SyscallResult handle_register_named_channel(uint64_t cookie, char *name) {
    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_register(cookie, name)) {
        return SYSCALL_OK;
    } else {
        return SYSCALL_BAD_NUMBER;
    }
}

SyscallResult handle_deregister_named_channel(char *name) {
    if (!IS_USER_ADDRESS(name)) {
        return SYSCALL_BADARGS;
    }

    if (named_channel_deregister(name) != 0) {
        return SYSCALL_OK;
    } else {
        return SYSCALL_BAD_NAME;
    }
}

SyscallResult handle_find_named_channel(char *name) {
    if (!IS_USER_ADDRESS(name)) {
        return 0;
    }

    return named_channel_find(name);
}

SyscallResult handle_syscall_69(SyscallArg arg0, SyscallArg arg1,
                                SyscallArg arg2, SyscallArg arg3,
                                SyscallArg arg4, SyscallArg syscall_num) {
    switch (syscall_num) {
#ifdef DEBUG_TEST_SYSCALL
    case 0:
        return handle_anos_testcall(arg0, arg1, arg2, arg3, arg4);
#endif
    case 1:
        return handle_debugprint((char *)arg0);
    case 2:
        return handle_debugchar((char)arg0);
    case 3:
        return handle_create_thread((ThreadFunc)arg0, (uintptr_t)arg1);
    case 4:
        return handle_memstats((AnosMemInfo *)arg0);
    case 5:
        return handle_sleep(arg0);
    case 6:
        return handle_create_process(arg0, arg1, arg2,
                                     (ProcessMemoryRegion *)arg3, arg4);
    case 7:
        return handle_map_virtual(arg0, arg1);
    case 8:
        return handle_send_message(arg0, arg1, arg2, (void *)arg3);
    case 9:
        return handle_recv_message(arg0, (uint64_t *)arg1, (size_t *)arg2,
                                   (void *)arg3);
    case 10:
        return handle_reply_message(arg0, arg1);
    case 11:
        return handle_create_channel();
    case 12:
        return handle_destroy_channel(arg0);
    case 13:
        return handle_register_named_channel(arg0, (char *)arg1);
    case 14:
        return handle_deregister_named_channel((char *)arg0);
    case 15:
        return handle_find_named_channel((char *)arg0);
    default:
        return SYSCALL_BAD_NUMBER;
    }
}