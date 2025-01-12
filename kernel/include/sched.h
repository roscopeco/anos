/*
 * stage3 - Scheduler interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_SCHED_H
#define __ANOS_KERNEL_SCHED_H

#include <stdbool.h>
#include <stdint.h>

bool sched_init(uintptr_t sys_sp, uintptr_t sys_ssp, uintptr_t start_func);

// This **must** be called with interrupts disabled!
void sched_schedule();

#endif //__ANOS_KERNEL_SCHED_H
