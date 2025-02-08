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

void sched_lock(void) {
    disable_interrupts();

    PerCPUState *cpu_state = state_get_per_cpu();

    if (cpu_state->irq_disable_count == 0) {
        spinlock_lock(&cpu_state->sched_lock);
    }

    cpu_state->irq_disable_count++;
}

void sched_unlock() {
    PerCPUState *cpu_state = state_get_per_cpu();

    if (cpu_state->irq_disable_count <= 1) {
        cpu_state->irq_disable_count = 0;
        spinlock_unlock(&cpu_state->sched_lock);
        enable_interrupts();
    } else {
        cpu_state->irq_disable_count--;
    }
}