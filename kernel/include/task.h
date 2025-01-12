/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_TASK_H
#define __ANOS_KERNEL_TASK_H

#include "process.h"
#include "structs/list.h"
#include <stdint.h>

#define DEFAULT_TIMESLICE ((20))

/*
 * task_switch.asm depends on the exact layout of this!
 */
typedef struct {
    ListNode this;  // 16 bytes
    uintptr_t tid;  // 24
    uintptr_t esp0; // 32
    uintptr_t ssp;  // 40
    Process *owner; // 48
    uintptr_t
            pml4; // 56 [duplicated from process to avoid cache miss on naive switch]
    uint64_t reserved2; // 64
} Task;

void task_init(void *tss);
Task *task_current();
void task_switch(Task *next);
Task *task_create_new(Process *owner, uintptr_t sp, uintptr_t func);

#endif //__ANOS_KERNEL_TASK_H