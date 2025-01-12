/*
 * stage3 - Naive round-robin to get us going...
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <ktypes.h>
#include <pmm/sys.h>
#include <slab/alloc.h>
#include <task.h>

Task *runnable_head;

#ifdef UNIT_TESTS
Task *test_sched_rr_get_runnable_head() { return runnable_head; }
#endif

bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func) {
    if (sys_ssp == 0) {
        return false;
    }

    // Create a process & task to represent the init thread (which System will inherit)
    Process *new_process = slab_alloc_block();
    new_process->pid = 1;
    new_process->pml4 = get_pagetable_root();

    Task *new_task = slab_alloc_block();

    // push address of init func as first place this task will return to...
    sys_ssp -= 8;
    *((uint64_t *)sys_ssp) = start_func;
    sys_ssp -= 120; // space for initial registers

    new_task->owner = new_process;
    new_task->pml4 = new_process->pml4;
    new_task->sp = sys_sp;
    new_task->ssp = sys_ssp;
    new_task->tid = 1;

    new_task->this.next = (ListNode *)new_task;
    new_task->this.type = KTYPE_TASK;

    runnable_head = new_task;

    return true;
}

void sched_schedule() { task_switch(runnable_head); }
