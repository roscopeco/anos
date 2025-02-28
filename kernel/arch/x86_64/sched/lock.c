/*
 * stage3 - Scheduler locking
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO this needs making properly safe for SMP...
 * 
 * TODO we are using CPU-local threads, might just need to cli in 
 *      the general case, and only spin when tweaking with other
 *      CPU's queues :thinking:
 */

#include <stdbool.h>
#include <stdint.h>

#include "machine.h"
#include "smp/state.h"
#include "spinlock.h"
#include "task.h"

#ifdef DEBUG_SCHED_LOCKS
#include "debugprint.h"
#define debuglog debugstr
#else
#define debuglog(...)
#endif

uint64_t sched_lock_any_cpu(PerCPUState *cpu_state) {
#ifdef EXPERIMENTAL_SCHED_LOCK
    return spinlock_lock_irqsave(&cpu_state->sched_lock_this_cpu);
#else
    if (cpu_state == state_get_for_this_cpu()) {
        debuglog("==> LOCK THIS\n");
        // If target is this CPU, we need to disable interrupts and lock
        disable_interrupts();

        if (cpu_state->irq_disable_count == 0) {
            spinlock_lock(&cpu_state->sched_lock_this_cpu);
        }

        cpu_state->irq_disable_count++;
    } else {
        debuglog("==> LOCK OTHER\n");
        // But for any other CPU, we can just lock...
        spinlock_lock(&cpu_state->sched_lock_this_cpu);
    }

    return 0;
#endif
}

uint64_t sched_lock_this_cpu(void) {
    return sched_lock_any_cpu(state_get_for_this_cpu());
}

void sched_unlock_any_cpu(PerCPUState *cpu_state, uint64_t lock_flags) {
#ifdef EXPERIMENTAL_SCHED_LOCK
    spinlock_unlock_irqrestore(&cpu_state->sched_lock_this_cpu, lock_flags);
#else
    if (cpu_state == state_get_for_this_cpu()) {
        debuglog("==> UNLOCK THIS\n");
        // If target is this CPU, we need to unlock and reenable interrupts
        if (cpu_state->irq_disable_count <= 1) {
            cpu_state->irq_disable_count = 0;
            spinlock_unlock(&cpu_state->sched_lock_this_cpu);
            enable_interrupts();
        } else {
            cpu_state->irq_disable_count--;
        }
    } else {
        // But for any other CPU, we can just unlock
        debuglog("==> UNLOCK OTHER\n");
        spinlock_unlock(&cpu_state->sched_lock_this_cpu);
    }
#endif
}

void sched_unlock_this_cpu(uint64_t lock_flags) {
    sched_unlock_any_cpu(state_get_for_this_cpu(), lock_flags);
}
