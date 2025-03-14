/*
 * stage3 - Task sleeping - initial experiment
 * anos - An Operating System
 * 
 * This is just experimental code, it won't be sticking
 * around in this form...
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_SLEEP_H
#define __ANOS_KERNEL_SLEEP_H

#include <stdint.h>

#include "task.h"

/* MUST call this _on each CPU_ before using any other sleep funcs! */
void sleep_init(void);

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t ticks);

/* Caller MUST lock the scheduler! */
void check_sleepers();

#endif //__ANOS_KERNEL_SLEEP_H