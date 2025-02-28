/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 * 
 * NOTE: The calls defined herein are currently only for test/debug.
 * They are poorly designed and **won't** be sticking around.
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "fba/alloc.h"
#include "kprintf.h"
#include "pmm/pagealloc.h"
#include "printhex.h"
#include "process.h"
#include "process/address_space.h"
#include "sched.h"
#include "sleep.h"
#include "smp/state.h"
#include "syscalls.h"
#include "task.h"
#include "vmm/vmconfig.h"

typedef void (*ThreadFunc)(void);

extern MemoryRegion *physical_region;

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

static SyscallResult handle_debugprint(char *message) {
    if (((uint64_t)message & 0xffffffff00000000) == 0) {
        kprintf(message);
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
    if (((uint64_t)mem_info & 0xffffffff00000000) == 0) {
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

SyscallResult handle_syscall_69(SyscallArg arg0, SyscallArg arg1,
                                SyscallArg arg2, SyscallArg arg3,
                                SyscallArg arg4, SyscallArg syscall_num) {
    switch (syscall_num) {
    case 0:
        return handle_anos_testcall(arg0, arg1, arg2, arg3, arg4);
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
    default:
        return SYSCALL_BAD_NUMBER;
    }
}