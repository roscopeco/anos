/*
 * stage3 - Prioritised round-robin, _slightly_ better than before...
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "anos_assert.h"
#include "debugprint.h"
#include "fba/alloc.h"
#include "pmm/sys.h"
#include "printhex.h"
#include "process.h"
#include "slab/alloc.h"
#include "smp/state.h"
#include "structs/pq.h"
#include "task.h"

#ifdef CONSERVATIVE_BUILD
#include "panic.h"
#endif

#ifdef UNIT_TESTS
#ifdef DEBUG_UNIT_TESTS
#include <stdio.h>
#else
#ifndef NULL
#define NULL (((void *)0))
#endif
#define printf(...)
#endif
#else
#ifndef NULL
#define NULL (((void *)0))
#endif
#define printf(...)
#endif

typedef per_cpu struct {
    TaskPriorityQueue realtime_head;
    TaskPriorityQueue high_head;
    TaskPriorityQueue normal_head;
    TaskPriorityQueue idle_head;
    uint64_t all_queue_total;
} PerCPUSchedState;

static_assert_sizeof(PerCPUSchedState, <=, STATE_SCHED_DATA_MAX);

#ifdef DEBUG_TASK_SWITCH
#include "debugprint.h"
#include "printhex.h"
#ifdef VERY_NOISY_TASK_SWITCH
#define vdebug(...) debugstr(__VA_ARGS__)
#define vdbgx64(arg) printhex64(arg, debugchar)
#define vdbgx8(arg) printhex8(arg, debugchar)
#else
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx8(...)
#endif
#define tdebug(...) debugstr(__VA_ARGS__)
#define tdbgx64(arg) printhex64(arg, debugchar)
#define tdbgx8(arg) printhex64(arg, debugchar)
#else
#define tdebug(...)
#define tdbgx64(...)
#define tdbgx8(...)
#define vdebug(...)
#define vdbgx64(...)
#define vdbgx8(...)
#endif

static Process *system_process;

void sched_idle_thread(void);

static inline PerCPUSchedState *get_this_cpu_sched_state(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();
    return (PerCPUSchedState *)cpu_state->sched_data;
}

static inline PerCPUSchedState *get_any_cpu_sched_state(uint8_t cpu_num) {
    PerCPUState *cpu_state = state_get_for_any_cpu(cpu_num);
    return (PerCPUSchedState *)cpu_state->sched_data;
}

static inline PerCPUSchedState *init_this_cpu_sched_state(void) {
    PerCPUSchedState *state = get_this_cpu_sched_state();

    task_pq_init(&state->realtime_head);
    task_pq_init(&state->high_head);
    task_pq_init(&state->normal_head);
    task_pq_init(&state->idle_head);

    state->all_queue_total = 0;

    return state;
}

static bool sched_enqueue_on(Task *task, PerCPUSchedState *cpu) {
    TaskPriorityQueue *candidate_queue = NULL;

    printf("REQUEUE\n");

    switch (task->sched->class) {
    case TASK_CLASS_REALTIME:
        printf("ENQUEUE realtime");
        candidate_queue = &cpu->realtime_head;
        break;
    case TASK_CLASS_HIGH:
        printf("ENQUEUE high");
        candidate_queue = &cpu->high_head;
        break;
    case TASK_CLASS_NORMAL:
        printf("ENQUEUE normal");
        candidate_queue = &cpu->normal_head;
        break;
    case TASK_CLASS_IDLE:
        printf("ENQUEUE idle");
        candidate_queue = &cpu->idle_head;
        break;
    default:
        vdebug("WARN: Attempt to enqueue task with bad class; Ignored\n");
        return false;
    }

    cpu->all_queue_total++;
    task_pq_push(candidate_queue, task);
    return true;
}

static bool sched_enqueue(Task *task) {
    return sched_enqueue_on(task, get_this_cpu_sched_state());
}

#ifdef UNIT_TESTS
// TODO there's too much test code leaking into prod code...
//
Task *test_sched_prr_get_runnable_head(TaskClass level) {
    switch (level) {
    case TASK_CLASS_REALTIME:
        return task_pq_peek(&get_this_cpu_sched_state()->realtime_head);
    case TASK_CLASS_HIGH:
        return task_pq_peek(&get_this_cpu_sched_state()->high_head);
    case TASK_CLASS_NORMAL:
        return task_pq_peek(&get_this_cpu_sched_state()->normal_head);
    case TASK_CLASS_IDLE:
        return task_pq_peek(&get_this_cpu_sched_state()->idle_head);
    }

    return NULL;
}

Task *test_sched_prr_set_runnable_head(TaskClass level, Task *task) {
    TaskPriorityQueue *queue;

    switch (level) {
    case TASK_CLASS_REALTIME:
        queue = &get_this_cpu_sched_state()->realtime_head;
        break;
    case TASK_CLASS_HIGH:
        queue = &get_this_cpu_sched_state()->high_head;
        break;
    case TASK_CLASS_NORMAL:
        queue = &get_this_cpu_sched_state()->normal_head;
        break;
    case TASK_CLASS_IDLE:
        queue = &get_this_cpu_sched_state()->idle_head;
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

// This should only be called on the BSP
bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func,
                uintptr_t bootstrap_func, TaskClass task_class) {

    if (sys_ssp == 0) {
        return false;
    }

    // Init the scheduler spinlock
    PerCPUState *cpu_state = state_get_for_this_cpu();
    spinlock_init(&cpu_state->sched_lock_this_cpu);

    // Set up our state...
    PerCPUSchedState *state = init_this_cpu_sched_state();

    // Create a process & task to represent the init thread (which System will inherit)
    Process *new_process = process_create(get_pagetable_root());

    if (new_process == NULL) {
        return false;
    }

    Task *new_task = task_create_new(new_process, sys_sp, sys_ssp,
                                     bootstrap_func, start_func, task_class);

    // During init it's just us, no need to lock / unlock
    if (!sched_enqueue(new_task)) {
        process_destroy(new_process);
        return false;
    }

    system_process = new_process;

    return true;
}

#ifdef DEBUG_SLEEPY_KERNEL_TASK
#include "fba/alloc.h"
#include "kprintf.h"
#include "sleep.h"
#include "task.h"

// TODO It seems there's a bug here (or possibly in kernel tasks generally).
//      When sleepy task is enabled, it _looks_ like user tasks start up
//      multiple times. This may be true, or it may be a terminal artifact
//      (though it does occur on the serial term too).
//
//      Investigate. Check the entrypoint and how we're handling the locks
//      especially, it feels very much like a straight-up race condition...
//
void sleepy_kernel_task(void) {
    PerCPUState *state = state_get_for_this_cpu();

    while (1) {
        kprintf("    Hello from #%ld    ", state->cpu_id);
        sleep_task(task_current(), 5000000000 + (1000000000 * state->cpu_id));
    }

    __builtin_unreachable();
}

static bool init_sleepy_kernel_task(PerCPUSchedState *state,
                                    uintptr_t bootstrap_func) {
    uintptr_t sleepy_sstack = (uintptr_t)fba_alloc_block();
    if (!sleepy_sstack) {
        return false;
    }

    uintptr_t sleepy_ustack = (uintptr_t)fba_alloc_block();
    if (!sleepy_ustack) {
        fba_free((void *)sleepy_sstack);
        return false;
    }

    Task *sleepy_task = task_create_kernel(
            system_process, sleepy_ustack + 0x1000, sleepy_sstack + 0x1000,
            (uint64_t)sleepy_kernel_task, TASK_CLASS_NORMAL);

    if (!sleepy_task) {
        fba_free((void *)sleepy_sstack);
        fba_free((void *)sleepy_ustack);
        return false;
    }

    state->all_queue_total += 1;
    task_pq_push(&state->normal_head, sleepy_task);

    return true;
}
#else
#define init_sleepy_kernel_task(...) true
#endif

bool sched_init_idle(uintptr_t sp, uintptr_t sys_ssp,
                     uintptr_t bootstrap_func) {

    if (!system_process) {
        return false;
    }

    PerCPUSchedState *state = get_this_cpu_sched_state();

    Task *idle_task =
            task_create_new(system_process, sp, sys_ssp, bootstrap_func,
                            (uintptr_t)sched_idle_thread, TASK_CLASS_IDLE);

    if (!sched_enqueue(idle_task)) {
        task_destroy(idle_task);
        return false;
    }

    return init_sleepy_kernel_task(state, bootstrap_func);
}

void sched_schedule(void) {
    PerCPUSchedState *state = get_this_cpu_sched_state();

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
        if (current->sched->ts_remain > 0) {
            --current->sched->ts_remain;
        }

#ifdef CONSERVATIVE_BUILD
        if (current->sched->state == TASK_STATE_TERMINATED) {
            panic("[BUG] A terminated task is running!");
        }
#endif

        if (current->sched->state != TASK_STATE_BLOCKED &&
            current->sched->state != TASK_STATE_TERMINATING &&
            current->sched->ts_remain > 0 &&
            (candidate_next->sched->class <= current->sched->class ||
             (candidate_next->sched->class == current->sched->class &&
              candidate_next->sched->prio >= current->sched->prio))) {
            printf("TIMESLICE CONTINUES\n");
            // timeslice continues, and nothing higher priority is preempting, so stick with it
            vdebug("No preempting task, and  ");
            vdbgx64((uint64_t)current);
            vdebug(" still has ");
            vdbgx64((uint64_t)current->sched->ts_remain);
            vdebug(" ticks left to run...\n");
            return;
        }
    }

    // Now we know we're going to switch, we can actually dequeue
    Task *next = task_pq_pop(candidate_queue);
    state->all_queue_total -= 1;

    vdebug("Switch to ");
    vdbgx64((uintptr_t)next);
    vdebug(" [TID = ");
    vdbgx64((uint64_t)next->sched->tid);
    vdebug("]\n");

    if (current && current->sched->state == TASK_STATE_RUNNING) {
        current->sched->state = TASK_STATE_READY;
        sched_enqueue(current);
    } else if (current && current->sched->state == TASK_STATE_TERMINATING) {
        current->sched->state = TASK_STATE_TERMINATED;
    }

    next->sched->ts_remain = DEFAULT_TIMESLICE;
    next->sched->state = TASK_STATE_RUNNING;

    task_switch(next);
}

PerCPUState *sched_find_target_cpu() {
    // TODO affinity
    PerCPUState *target = NULL;

    for (int i = 0; i < state_get_cpu_count(); i++) {
        PerCPUState *candidate = state_get_for_any_cpu(i);

        PerCPUSchedState *target_sched = NULL;
        if (target) {
            target_sched = (PerCPUSchedState *)target->sched_data;
        }

        PerCPUSchedState *candidate_sched =
                (PerCPUSchedState *)candidate->sched_data;

#ifdef CONSERVATIVE_BUILD
        if (candidate_sched == NULL) {
#ifdef CONSERVATIVE_PANICKY
            panic("[BUG] Candidate CPU Sched State is NULL");
#else
            debugstr("!!! WARN: [BUG] sched_find_target_cpu has NULL sched "
                     "state for CPU #");
            printhex8(candidate->cpu_num, debugchar);
            debugstr("\n");
#endif
        }
#endif

#ifdef TARGET_CPU_CONSIDER_SLEEPERS
        if ((candidate_sched->all_queue_total + candidate->sleep_queue.count) ==
            1) {
#else
        if (candidate_sched->all_queue_total == 1) {
#endif
            // short-circuit for a candidate with only the idle thread

            vdebug("WILL UNBLOCK ON CPU #");
            vdbgx8(candidate->cpu_id);
            vdebug(" which has ");
            vdbgx8(candidate_sched->all_queue_total);
            vdebug(" queued tasks\n");

            return candidate;
        }

        if (target == NULL ||
#ifdef TARGET_CPU_CONSIDER_SLEEPERS
            ((candidate_sched->all_queue_total + candidate->sleep_queue.count) <
             (target_sched->all_queue_total + candidate->sleep_queue.count))) {
#else
            (candidate_sched->all_queue_total <
             target_sched->all_queue_total)) {
#endif
            target = candidate;
        }
    }

#ifdef CONSERVATIVE_BUILD
    if (target == NULL) {
        // Only reason to be in here is if CPU count is < 1, which would be weird...
#ifdef CONSERVATIVE_PANICKY
        panic("[BUG] No target CPU found for schedule");
#else
        debugstr("!!! WARN: [BUG] sched_find_target_cpu is returning NULL!\n");
#endif
    }
#endif

    PerCPUSchedState *target_sched = (PerCPUSchedState *)target->sched_data;
    tdebug("WILL UNBLOCK ON CPU #");
    tdbgx8(target->cpu_id);
    tdebug(" which has ");
    tdbgx8(target_sched->all_queue_total);
    tdebug(" queued tasks\n");

    return target;
}

void sched_unblock_on(Task *task, PerCPUState *target_cpu_state) {
    task->sched->state = TASK_STATE_READY;
    bool result = sched_enqueue_on(
            task, ((PerCPUSchedState *)target_cpu_state->sched_data));

#ifdef CONSERVATIVE_BUILD
    if (!result) {
        debugstr("WARN: Failed to requeue unblocked task @");
        printhex64((uintptr_t)task, debugchar);
        debugstr("\n");
#ifdef CONSERVATIVE_PANICKY
        panic("Failed to requeue unblocked task");
#endif
    }
#endif
}

void sched_unblock(Task *task) {
    sched_unblock_on(task, state_get_for_this_cpu());
}

void sched_block(Task *task) { task->sched->state = TASK_STATE_BLOCKED; }