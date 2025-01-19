/*
 * stage3 - Task sleeping - initial experiment
 * anos - An Operating System
 * 
 * This is just experimental code, it won't be sticking
 * around in this form...
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "sched.h"
#include "structs/list.h"
#include "task.h"
#include <stdint.h>

/* Caller MUST lock the scheduler! */
void sleep_task(Task *task, uint64_t ticks);

/* Caller MUST lock the scheduler! */
void check_sleepers();