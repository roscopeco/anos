/*
 * stage3 - Scheduler interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SCHED_H
#define __ANOS_KERNEL_SCHED_H

#include "task.h"
#include <stdbool.h>
#include <stdint.h>

// This **must** only be called on the BSP
bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func,
                uintptr_t bootstrap_func, TaskClass task_class);

// This **must** be called on all CPUs, **after** sched_init
// and before the first schedule...
bool sched_init_idle(uintptr_t sys_ssp, uintptr_t bootstrap_func);

// This **must** be called with the scheduler locked and interrupts disabled!
// sched_lock / sched_unlock will take care of that.
void sched_schedule(void);

// This **must** be called with the scheduler locked and interrupts disabled!
void sched_block(Task *task);

// This **must** be called with the scheduler locked and interrupts disabled!
void sched_unblock(Task *task);

void sched_lock(void);
void sched_unlock(void);

#endif //__ANOS_KERNEL_SCHED_H
