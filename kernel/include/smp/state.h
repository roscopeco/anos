/*
 * stage3 - SMP per-CPU state
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_SMP_STATE_H
#define __ANOS_SMP_STATE_H

#include <stdint.h>

#include "anos_assert.h"
#include "sleep_queue.h"
#include "spinlock.h"
#include "vmm/vmconfig.h"

#define STATE_SCHED_DATA_MAX ((672))
#define STATE_TASK_DATA_MAX ((32))

// This is just a marker define for information purposes...
#define per_cpu

typedef struct PerCPUState {
    struct PerCPUState *self; // 8
    uint64_t cpu_id;          // 16
    uint64_t lapic_id;        // 24
    uint64_t *lapic;          // 32
    uint8_t reserved0[32];    // takes us to 64 bytes

    char cpu_brand[49];
    uint8_t reserved1[15]; // takes us to 128 bytes

    SpinLock sched_lock; // 192 (keep this aligned on 64-byte for cache line!)
    uint8_t irq_disable_count; // 196

    uint8_t reserved2[63]; // takes us to 256 bytes

    uint8_t sched_data[STATE_SCHED_DATA_MAX]; // takes us to 928 bytes
    uint8_t task_data[STATE_TASK_DATA_MAX];   // takes us to 960 bytes

    SleepQueue sleep_queue; // takes us to 1024 bytes

    uint8_t reserved3[3072]; // takes us to 4096 bytes
} PerCPUState;

static_assert_sizeof(PerCPUState, ==, VM_PAGE_SIZE);

#ifdef UNIT_TESTS
#ifdef MUNIT_H
PerCPUState __test_cpu_state;
#endif

static inline PerCPUState *state_get_per_cpu(void) {
#ifndef MUNIT_H
    extern PerCPUState __test_cpu_state;
#endif
    return &__test_cpu_state;
}
#else
// Assumes GS is already swapped to KernelGSBase...
static inline PerCPUState *state_get_per_cpu(void) {
    PerCPUState *ptr;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(ptr));
    return ptr;
}
#endif

#endif //__ANOS_SMP_STATE_H
