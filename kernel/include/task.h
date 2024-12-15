/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_TASK_H
#define __ANOS_KERNEL_TASK_H

#include <stdint.h>

/*
 * task_switch.asm depends on the exact layout of this!
 * Make sure it only grows, and stays packed...
 */
typedef struct {
    uintptr_t tid;
    uintptr_t sp;
} Task;

Task *task_current();
void task_switch(Task *next);

#ifdef DEBUG_TEST_TASKS
#include <stdnoreturn.h>
noreturn void debug_test_tasks();
#endif

#endif //__ANOS_KERNEL_TASK_H