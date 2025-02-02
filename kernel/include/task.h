/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_TASK_H
#define __ANOS_KERNEL_TASK_H

#include "anos_assert.h"
#include "process.h"
#include "structs/list.h"
#include <stdint.h>

#define DEFAULT_TIMESLICE (((uint8_t)20))

typedef enum {
    TASK_CLASS_IDLE = 0,
    TASK_CLASS_NORMAL,
    TASK_CLASS_HIGH,
    TASK_CLASS_REALTIME,
} __attribute__((packed)) TaskClass;

typedef enum {
    TASK_STATE_BLOCKED = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
} __attribute__((packed)) TaskState;

/*
 * task_switch.asm depends on the exact layout of this!
 */
typedef struct Task {
    ListNode this;  // 16 bytes
    uintptr_t tid;  // 24
    uintptr_t rsp0; // 32
    uintptr_t ssp;  // 40
    Process *owner; // 48

    // duplicated from process to avoid cache miss on naive switch
    uintptr_t pml4; // 56

    uint16_t ts_remain; // 58
    TaskState state;    // 59
    TaskClass class;    // 60
    uint8_t prio;       // 61
    uint8_t res1;       // 62
    uint16_t res2;      // 64
} __attribute__((packed)) Task;

static_assert_sizeof(Task, ==, 64);

void task_init(void *tss);
Task *task_current();
void task_switch(Task *next);

/* 
 * Create a new user task with the specified process, stacks and entrypoint.
 *
 * NOTE: sys_ssp may be 0, which will cause a new stack to be allocated.
 */
Task *task_create_new(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                      uintptr_t bootstrap, uintptr_t func, TaskClass class);

Task *task_create_user(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                       uintptr_t func, TaskClass class);

Task *task_create_kernel(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                         uintptr_t func, TaskClass class);

#endif //__ANOS_KERNEL_TASK_H