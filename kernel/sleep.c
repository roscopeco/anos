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
#include "kprintf.h"
#endif

#ifndef NULL
#define NULL (((void *)0))
#endif

uint64_t get_kernel_upticks(void);

void sleep_init(void) {
    PerCPUState *cpu_state = state_get_for_this_cpu();
    sleep_queue_init(&cpu_state->sleep_queue);
}

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t nanos) {
    if (task != NULL) {
        PerCPUState *cpu_state = state_get_for_this_cpu();

        uint64_t wake_tick = get_kernel_upticks() + (nanos / NANOS_PER_TICK);
        sleep_queue_enqueue(&cpu_state->sleep_queue, task, wake_tick);

#ifdef DEBUG_SLEEP
        kprintf("Sleep 0x%016lx\n    => Ticks now is 0x%016lx - Will wake at "
                "0x%016lx\n",
                (uintptr_t)task, get_kernel_upticks(), wake_tick);
#endif
        sched_block(task);
        sched_schedule();
    }
}

/* Caller MUST lock the scheduler! */
void check_sleepers() {
#ifdef DEBUG_SLEEP
#ifdef VERY_NOISY_SLEEP
    if (get_kernel_upticks() % 100 == 0) {
        kprintf("check_sleepers(): Ticks now is 0x%016lx\n", get_kernel_upticks());
    }
#endif
#endif

    PerCPUState *cpu_state = state_get_for_this_cpu();
    Task *waker = sleep_queue_dequeue(&cpu_state->sleep_queue, get_kernel_upticks());

    while (waker) {
        Task *next = (Task *)waker->this.next;
#ifdef SLEEP_SCHED_ONLY_THIS_CPU
        sched_unblock(waker);
#else
        PerCPUState *target_cpu = sched_find_target_cpu();

#ifdef DEBUG_SLEEP
        kprintf("\n    => WAKE 0x%016lx (PID 0x%016lx) on CPU 0x%016lx\n", (uintptr_t)waker, waker->sched->tid,
                target_cpu->cpu_id);
#endif
        if (target_cpu != cpu_state) {
            uint64_t lock_flags = sched_lock_any_cpu(target_cpu);
            sched_unblock_on(waker, target_cpu);
            sched_unlock_any_cpu(target_cpu, lock_flags);
        } else {
            // Scheduler already locked on this CPU...
            sched_unblock_on(waker, target_cpu);
        }
#endif
        waker = next;
    }
}