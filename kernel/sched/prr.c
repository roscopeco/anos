/*
 * stage3 - Prioritised round-robin, _slightly_ better than before...
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "anos_assert.h"
#include "ktypes.h"
#include "pmm/sys.h"
#include "slab/alloc.h"
#include "smp/state.h"
#include "structs/pq.h"
#include "task.h"

#ifdef DEBUG_UNIT_TESTS
#include <stdio.h>
#else
#define NULL (((void *)0))
#define printf(...)
#endif

typedef per_cpu struct {
    TaskPriorityQueue realtime_head;
    TaskPriorityQueue high_head;
    TaskPriorityQueue normal_head;
    TaskPriorityQueue idle_head;
} PerCPUSchedState;

static_assert_sizeof(PerCPUSchedState, <=, STATE_SCHED_DATA_MAX);

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

static inline PerCPUSchedState *get_cpu_sched_state(void) {
    PerCPUState *cpu_state = state_get_per_cpu();
    return (PerCPUSchedState *)cpu_state->sched_data;
}

static inline PerCPUSchedState *init_cpu_sched_state(void) {
    PerCPUSchedState *state = get_cpu_sched_state();

    task_pq_init(&state->realtime_head);
    task_pq_init(&state->high_head);
    task_pq_init(&state->normal_head);
    task_pq_init(&state->idle_head);

    return state;
}

#ifdef UNIT_TESTS
Task *test_sched_prr_get_runnable_head(TaskClass level) {
    switch (level) {
    case TASK_CLASS_REALTIME:
        return task_pq_peek(&get_cpu_sched_state()->realtime_head);
    case TASK_CLASS_HIGH:
        return task_pq_peek(&get_cpu_sched_state()->high_head);
    case TASK_CLASS_NORMAL:
        return task_pq_peek(&get_cpu_sched_state()->normal_head);
    case TASK_CLASS_IDLE:
        return task_pq_peek(&get_cpu_sched_state()->idle_head);
    }

    return NULL;
}

Task *test_sched_prr_set_runnable_head(TaskClass level, Task *task) {
    TaskPriorityQueue *queue;

    switch (level) {
    case TASK_CLASS_REALTIME:
        queue = &get_cpu_sched_state()->realtime_head;
        break;
    case TASK_CLASS_HIGH:
        queue = &get_cpu_sched_state()->high_head;
        break;
    case TASK_CLASS_NORMAL:
        queue = &get_cpu_sched_state()->normal_head;
        break;
    case TASK_CLASS_IDLE:
        queue = &get_cpu_sched_state()->idle_head;
        break;
    default:
        return NULL;
    }

    Task *old = queue->head;
    if (task) {
        task->this.next = (ListNode *)old;
    }
    queue->head = task;

    return old;
}
#endif

bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func,
                uintptr_t bootstrap_func, TaskClass task_class) {
    if (sys_ssp == 0) {
        return false;
    }

    // Init the scheduler spinlock
    PerCPUState *cpu_state = state_get_per_cpu();
    spinlock_init(&cpu_state->sched_lock);

    // Set up our state...
    PerCPUSchedState *state = init_cpu_sched_state();

    TaskPriorityQueue *queue;
    switch (task_class) {
    case TASK_CLASS_REALTIME:
        queue = &state->realtime_head;
        break;
    case TASK_CLASS_HIGH:
        queue = &state->high_head;
        break;
    case TASK_CLASS_NORMAL:
        queue = &state->normal_head;
        break;
    case TASK_CLASS_IDLE:
        queue = &state->idle_head;
        break;
    default:
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
    *((uint64_t *)sys_ssp) = (uint64_t)bootstrap_func;

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
    new_task->prio = 0;
    new_task->class = task_class;

    new_task->this.next = NULL;
    new_task->this.type = KTYPE_TASK;

    task_pq_push(queue, new_task);

    return true;
}

static void sched_enqueue(Task *task) {
    PerCPUSchedState *state = get_cpu_sched_state();
    TaskPriorityQueue *candidate_queue = NULL;

    printf("REQUEUE\n");

    switch (task->class) {
    case TASK_CLASS_REALTIME:
        candidate_queue = &state->realtime_head;
        break;
    case TASK_CLASS_HIGH:
        candidate_queue = &state->high_head;
        break;
    case TASK_CLASS_NORMAL:
        candidate_queue = &state->normal_head;
        break;
    case TASK_CLASS_IDLE:
        candidate_queue = &state->idle_head;
        break;
    default:
        vdebug("WARN: Attempt to requeue task with bad class; Ignored\n");
        return;
    }

    task_pq_push(candidate_queue, task);
}

void sched_schedule(void) {
    PerCPUSchedState *state = get_cpu_sched_state();

    Task *current = task_current();
    Task *candidate_next = NULL;
    TaskPriorityQueue *candidate_queue = NULL;

    vdebug("Switching tasks : current is ");
    vdbgx64((uintptr_t)current);
    vdebug("\n");

    if ((candidate_next = task_pq_peek(&state->realtime_head))) {
        printf("Have a realtime candidate\n");
        candidate_queue = &state->realtime_head;
    } else if ((candidate_next = task_pq_peek(&state->high_head))) {
        printf("Have a high candidate\n");
        candidate_queue = &state->high_head;
    } else if ((candidate_next = task_pq_peek(&state->normal_head))) {
        printf("Have a normal candidate\n");
        candidate_queue = &state->normal_head;
    } else if ((candidate_next = task_pq_peek(&state->idle_head))) {
        printf("Have an idle candidate\n");
        candidate_queue = &state->idle_head;
    } else {
        printf("No candidate! Always current class is %d; Normal head is %p\n",
               always_current->class, normal_head.head);
    }

    printf("Candidate is %p\n", candidate_next);

    if (candidate_next == NULL) {
        // no more tasks, just carry on
        // not allocating another timeslice, so we'll still switch as soon as something else comes up...
        vdebug("No more tasks; Switch aborted\n");
        return;
    }

    // Decrement timeslice
    if (current) {
        if (current->ts_remain > 0) {
            --current->ts_remain;
        }

        if (current->state != TASK_STATE_BLOCKED && current->ts_remain > 0 &&
            (candidate_next->class <= current->class ||
             (candidate_next->class == current->class &&
              candidate_next->prio >= current->prio))) {
            printf("TIMESLICE CONTINUES\n");
            // timeslice continues, and nothing higher priority is preempting, so stick with it
            vdebug("No preempting task, and  ");
            vdbgx64((uint64_t)current);
            vdebug(" still has ");
            vdbgx64((uint64_t)current->ts_remain);
            vdebug(" ticks left to run...\n");
            return;
        }
    }

    // Now we know we're going to switch, we can actually dequeue
    Task *next = task_pq_pop(candidate_queue);

    tdebug("Switch to ");
    tdbgx64((uintptr_t)next);
    tdebug(" [TID = ");
    tdbgx64((uint64_t)next->tid);
    tdebug("]\n");

    if (current && current->state == TASK_STATE_RUNNING) {
        current->state = TASK_STATE_READY;
        sched_enqueue(current);
    }

    next->ts_remain = DEFAULT_TIMESLICE;
    next->state = TASK_STATE_RUNNING;

    task_switch(next);
}

void sched_unblock(Task *task) {
    task->state = TASK_STATE_READY;
    sched_enqueue(task);
}

void sched_block(Task *task) { task->state = TASK_STATE_BLOCKED; }