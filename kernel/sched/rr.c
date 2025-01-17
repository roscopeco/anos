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

static Task *runnable_head;

#define NULL (((void *)0))

#ifdef DEBUG_TASK_SWITCH
#include "debugprint.h"
#include "printhex.h"
#ifdef VERY_NOISY_TASK_SWITCH
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#else
#define vdebug(...)
#define vdbgx64(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#else
#define tdebug(...)
#define tdbgx64(...)
#define vdebug(...)
#define vdbgx64(...)
#endif

#ifdef UNIT_TESTS
Task *test_sched_rr_get_runnable_head() { return runnable_head; }
#endif

void user_thread_entrypoint(void);

bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func) {
    if (sys_ssp == 0) {
        return false;
    }

    // Create a process & task to represent the init thread (which System will inherit)
    Process *new_process = slab_alloc_block();
    new_process->pid = 1;
    new_process->pml4 = get_pagetable_root();

    Task *new_task = slab_alloc_block();

    new_task->rsp0 = sys_ssp;

    // push address of init func as first place this task will return to...
    sys_ssp -= 8;
    *((uint64_t *)sys_ssp) = (uint64_t)user_thread_entrypoint;

    // space for initial registers except rsi, rdi, values don't care...
    sys_ssp -= 104;

    // push address of thread user stack, this will get popped into rsi...
    sys_ssp -= 8;
    *((uint64_t *)sys_ssp) = sys_sp;

    // push address of thread func, this will get popped into rdi...
    sys_ssp -= 8;
    *((uint64_t *)sys_ssp) = start_func;

    new_task->ssp = sys_ssp;
    new_task->owner = new_process;
    new_task->pml4 = new_process->pml4;
    new_task->tid = 1;
    new_task->ts_remain = DEFAULT_TIMESLICE;
    new_task->state = TASK_STATE_READY;

    new_task->this.next = NULL;
    new_task->this.type = KTYPE_TASK;

    runnable_head = new_task;

    return true;
}

void sched_schedule() {
    Task *current = task_current();

    vdebug("Switching tasks : current is ");
    vdbgx64((uintptr_t)current);
    vdebug("\n");

    // TODO this will be false _exactly once_ (the first time we switch),
    //      seems a shame to have to check it every time...
    if (current) {
        if (--current->ts_remain != 0) {
            // timeslice continues, stick with it
            vdebug("Task ");
            vdbgx64((uint64_t)current);
            vdebug(" still has ");
            vdbgx64((uint64_t)current->reserved2);
            vdebug(" ticks left to run...\n");
            return;
        }

        if (runnable_head == NULL) {
            // no more tasks, just carry on
            vdebug("No more tasks; Switch aborted\n");
            return;
        }
    } else {
        if (runnable_head == NULL) {
            // no more tasks, and apparently no current - warn
            // TODO panic?
            tdebug("WARN: Apparent corruption - no current task and no "
                   "runnable, probable crash incoming...\n");
            return;
        }
    }

    Task *next = runnable_head;

    tdebug("Switch to ");
    tdbgx64((uintptr_t)next);
    tdebug(" [TID = ");
    tdbgx64((uint64_t)next->tid);
    tdebug("]\n");

    runnable_head = (Task *)next->this.next;

    if (current && current->state == TASK_STATE_RUNNING) {
        current->state = TASK_STATE_READY;

        current = (Task *)list_add((ListNode *)runnable_head,
                                   (ListNode *)current);

        if (runnable_head == NULL) {
            runnable_head = current;
        }
    }

    next->ts_remain = DEFAULT_TIMESLICE;
    next->state = TASK_STATE_RUNNING;

    task_switch(next);
}

void sched_unblock(Task *task) {
    task = (Task *)list_add((ListNode *)runnable_head, (ListNode *)task);

    if (runnable_head == NULL) {
        runnable_head = task;
    }

    // TODO conditionally sched_schedule() once preemption is supported...
    task->state = TASK_STATE_READY;
}

void sched_block(Task *task) {
    task->state = TASK_STATE_BLOCKED;

    sched_schedule();
}