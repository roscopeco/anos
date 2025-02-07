/*
 * stage3 - Syscall implementations
 * anos - An Operating System
 * 
 * NOTE: The calls defined herein are currently only for test/debug.
 * They are poorly designed and **won't** be sticking around.
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "syscalls.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "pmm/pagealloc.h"
#include "printhex.h"
#include "sched.h"
#include "sleep.h"
#include "task.h"

#include <stdint.h>

typedef void (*ThreadFunc)(void);

extern MemoryRegion *physical_region;

static SyscallResult handle_testcall(SyscallArg arg0, SyscallArg arg1,
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
        debugstr(message);
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

    sched_lock();
    sched_unblock(task);
    sched_unlock();

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
    sched_lock();
    sleep_task(task_current(), nanos);
    sched_unlock();

    return SYSCALL_OK;
}

SyscallResult handle_syscall_69(SyscallArg arg0, SyscallArg arg1,
                                SyscallArg arg2, SyscallArg arg3,
                                SyscallArg arg4, SyscallArg syscall_num) {
    switch (syscall_num) {
    case 0:
        return handle_testcall(arg0, arg1, arg2, arg3, arg4);
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
    default:
        return SYSCALL_BAD_NUMBER;
    }
}