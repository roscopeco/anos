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

bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func);

// This **must** be called with the scheduler locked and interrupts disabled!
// sched_lock / sched_unlock will take care of that.
void sched_schedule(void);

void sched_lock(void);
void sched_unlock(void);

void sched_block(Task *task);
void sched_unblock(Task *task);

#endif //__ANOS_KERNEL_SCHED_H