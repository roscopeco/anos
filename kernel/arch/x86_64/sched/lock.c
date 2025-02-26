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
    return spinlock_lock_irqsave(&cpu_state->sched_lock_this_cpu);
}

uint64_t sched_lock_this_cpu(void) {
    return sched_lock_any_cpu(state_get_for_this_cpu());
}

void sched_unlock_any_cpu(PerCPUState *cpu_state, uint64_t lock_flags) {
    spinlock_unlock_irqrestore(&cpu_state->sched_lock_this_cpu, lock_flags);
}

void sched_unlock_this_cpu(uint64_t lock_flags) {
    sched_unlock_any_cpu(state_get_for_this_cpu(), lock_flags);
}
