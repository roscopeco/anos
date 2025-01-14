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

#define DEFAULT_TIMESLICE (((uint8_t)20))

typedef enum {
    TASK_STATE_BLOCKED = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
} TaskState;

/*
 * task_switch.asm depends on the exact layout of this!
 */
typedef struct {
    ListNode this;  // 16 bytes
    uintptr_t tid;  // 24
    uintptr_t rsp0; // 32
    uintptr_t ssp;  // 40
    Process *owner; // 48

    // duplicated from process to avoid cache miss on naive switch
    uintptr_t pml4; // 56

    uint16_t ts_remain; // 58
    uint8_t state;      // 59
    uint8_t res1;       // 60
    uint32_t res2;      // 64
} __attribute__((packed)) Task;

void task_init(void *tss);
Task *task_current();
void task_switch(Task *next);
Task *task_create_new(Process *owner, uintptr_t sp, uintptr_t func);

#endif //__ANOS_KERNEL_TASK_H