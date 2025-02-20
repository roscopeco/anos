/*
 * stage3 - Task sleeping - initial experiment
 * anos - An Operating System
 * 
 * This is just experimental code, it won't be sticking
 * around in this form...
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "config.h"
#include "sched.h"
#include "sleep_queue.h"
#include "smp/state.h"
#include "structs/list.h"
#include "task.h"

#ifdef DEBUG_SLEEP
#include "debugprint.h"
#include "printhex.h"
#endif

#define NULL (((void *)0))

uint64_t get_lapic_timer_upticks(void);

void sleep_init(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();
    sleep_queue_init(&cpu_state->sleep_queue);
}

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t nanos) {
    if (task != NULL) {
        PerCPUState *cpu_state = state_get_for_this_cpu();

        uint64_t wake_tick =
                get_lapic_timer_upticks() + (nanos / NANOS_PER_TICK);
        sleep_queue_enqueue(&cpu_state->sleep_queue, task, wake_tick);

#ifdef DEBUG_SLEEP
        debugstr("Ticks now is ");
        printhex64(get_lapic_timer_upticks(), debugchar);
        debugstr(" - Waking at ");
        printhex64(wake_tick, debugchar);
        debugstr("\n");
#endif
        sched_block(task);
        sched_schedule();
    }
}

/* Caller MUST lock the scheduler! */
void check_sleepers() {
#ifdef DEBUG_SLEEP
    if (get_lapic_timer_upticks() % 100 == 0) {
        debugstr("Ticks now is ");
        printhex64(get_lapic_timer_upticks(), debugchar);
        debugstr(" - Next wakeup at ");
        printhex64(wake_tick, debugchar);
        debugstr("\n");
    }
#endif

    PerCPUState *cpu_state = state_get_for_this_cpu();
    Task *waker = sleep_queue_dequeue(&cpu_state->sleep_queue,
                                      get_lapic_timer_upticks());

    while (waker) {
        Task *next = (Task *)waker->this.next;
#ifdef SLEEP_SCHED_ONLY_THIS_CPU
        sched_unblock(waker);
#else
        PerCPUState *target_cpu = sched_find_target_cpu();
        sched_lock_any_cpu(target_cpu);
        sched_unblock_on(waker, target_cpu);
        sched_unlock_any_cpu(target_cpu);
#endif
        waker = next;
    }
}