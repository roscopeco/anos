/*
 * stage3 - Tasks
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_TASK_H
#define __ANOS_KERNEL_TASK_H

#include "anos_assert.h"
#include "process.h"
#include "structs/list.h"
#include <stdint.h>

#define DEFAULT_TIMESLICE (((uint8_t)10))

/**
 * These define the number of registers that the arch-specific task switch
 * pushes and pops to the stack during a switch.
 * 
 * It should be the total number of 64-bit values that are pushed after the
 * return address.
 */
#if defined ARCH_X86_64
#define TASK_SAVED_REGISTER_COUNT ((15))
#elif defined ARCH_RISCV64
#define TASK_SAVED_REGISTER_COUNT ((31))
#elif defined UNIT_TESTS
#define TASK_SAVED_REGISTER_COUNT ((15))
#else
#error TASK_SAVED_REGISTER_COUNT not defined for this architecture in task.h!
#endif
#define TASK_SAVED_REGISTER_BYTES ((TASK_SAVED_REGISTER_COUNT * 8))

typedef enum {
    TASK_CLASS_IDLE = 0,
    TASK_CLASS_NORMAL,
    TASK_CLASS_HIGH,
    TASK_CLASS_REALTIME,
} __attribute__((packed)) TaskClass;

typedef enum {
    TASK_STATE_BLOCKED = 0, // Task is blocked on some condition
    TASK_STATE_READY,       // Task is ready to go
    TASK_STATE_RUNNING,     // Task is actively running
    TASK_STATE_TERMINATING, // Task is ready to terminate (may still be running)
    TASK_STATE_TERMINATED,  // Task is terminated - definitely not running
} __attribute__((packed)) TaskState;

// clang-format off
// These are bits in the flags member of TaskSched...
#define TASK_SCHED_FLAG_KILLED ((1 << 0))  // Trigger has been pulled
#define TASK_SCHED_FLAG_DYING ((1 << 1))   // Task is actively dying, or is dead (see TaskState for confirmation)
// clang-format on

/**
 * Task scheduler data - Stuff not needed in best-case fast
 * path (e.g. syscalls).
 */
typedef struct {
    uintptr_t tid;         // 8
    uint16_t ts_remain;    // 10
    TaskState state;       // 11
    TaskClass class;       // 12
    uint8_t prio;          // 13
    uint16_t status_flags; // 15
    uint8_t res2;          // 16
    uint64_t reserved[6];
} __attribute__((packed)) TaskSched;

/*
 * task_switch.asm (and init_syscalls.asm) depends on the 
 * exact layout of this!
 */
typedef struct Task {
    ListNode this;    // 8 bytes
    void *data;       // 16 - arch specific data (points into this struct)
    TaskSched *sched; // 24 - scheduler data (points into this struct)
    uintptr_t rsp0;   // 32
    uintptr_t ssp;    // 40
    Process *owner;   // 48

    // duplicated from process to avoid cache miss on naive switch
    uintptr_t pml4; // 56

    uintptr_t usp_stash; // 64

    TaskSched ssched;        // 128
    uint64_t reserved0[112]; // 1024
    uint8_t sdata[2048];     // 3072
    uint64_t reserved1[128]; // 4096
} __attribute__((packed)) Task;

static_assert_sizeof(Task, ==, 4096);

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

void task_destroy(Task *task);

Task *task_create_user(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                       uintptr_t func, TaskClass class);

Task *task_create_kernel(Process *owner, uintptr_t sp, uintptr_t sys_ssp,
                         uintptr_t func, TaskClass class);

void task_remove_from_process(Task *task);

/*
 * Must be called with scheduler locked!
 */
noreturn void task_current_exitpoint(void);

#endif //__ANOS_KERNEL_TASK_H