/*
 * stage3 - SMP per-CPU state
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * Be sure to keep this in-sync with state.inc!
 */

// clang-format Language: C

#ifndef __ANOS_SMP_STATE_H
#define __ANOS_SMP_STATE_H

#include <stdint.h>

#include "anos_assert.h"
#include "sleep_queue.h"
#include "spinlock.h"
#include "structs/shift_array.h"
#include "vmm/vmconfig.h"

#define STATE_SCHED_DATA_MAX ((672))
#define STATE_TASK_DATA_MAX ((32))

// This is just a marker define for information purposes...
#define per_cpu

// TODO should probably rejig this to be more cache friendly...
typedef struct PerCPUState {
    struct PerCPUState *self; // 8
    uint64_t cpu_id;          // 16
    uint64_t lapic_id;        // 24
    uint64_t *lapic;          // 32
    uint8_t reserved0[32];    // takes us to 64 bytes

    char cpu_brand[49];
    uint8_t reserved1[15]; // takes us to 128 bytes

    SpinLock
            sched_lock_this_cpu; // 192 (keep this aligned on 64-byte for cache line!)

    uint8_t irq_disable_count; // 193 TODO remove if EXPERIMENTAL_LOCK becomes permanent!
    uint8_t reserved2[63]; // 256

    uint8_t sched_data[STATE_SCHED_DATA_MAX]; // takes us to 928 bytes
    uint8_t task_data[STATE_TASK_DATA_MAX];   // takes us to 960 bytes

    uint64_t reserved3[8]; // takes us to 1024 bytes

    SleepQueue sleep_queue;            // 1088 (locked by sched lock)
    SpinLock ipwi_queue_lock_this_cpu; // 1152
    ShiftToMiddleArray ipwi_queue;     // 1216

    uint8_t reserved4[2880]; // takes us to 4096 bytes
} PerCPUState;

static_assert_sizeof(PerCPUState, ==, VM_PAGE_SIZE);

#ifdef UNIT_TESTS
#ifdef MUNIT_H
PerCPUState __test_cpu_state;
#else
extern PerCPUState __test_cpu_state;
static inline PerCPUState *state_get_for_this_cpu(void) {
    return &__test_cpu_state;
}
static inline uint8_t state_get_cpu_count(void) { return 1; }
static inline PerCPUState *state_get_for_any_cpu(uint8_t cpu_num) {
    return &__test_cpu_state;
}
#endif
#else

#if defined __x86_64__
#include "x86_64/smp/state.h"
#elif defined __riscv
#include "riscv64/smp/state.h"
#else
#error No arch defined for SMP state.h
#endif

#endif

void state_register_cpu(uint8_t cpu_num, PerCPUState *state);

uint8_t state_get_cpu_count(void);
PerCPUState *state_get_for_any_cpu(uint8_t cpu_num);

#endif //__ANOS_SMP_STATE_H
