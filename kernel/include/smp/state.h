/*
 * stage3 - SMP per-CPU state
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_SMP_STATE_H
#define __ANOS_SMP_STATE_H

#include <stdint.h>

#include "anos_assert.h"

typedef struct PerCPUState {
    struct PerCPUState *self; // 8
    uint64_t cpu_id;          // 16
    uint64_t lapic_id;        // 24
    uint64_t *lapic;          // 32
    uint8_t reserved0[32];    // takes us to 64 bytes

    char cpu_brand[49];
    uint8_t reserved1[143]; // takes us to 256 bytes

    uint8_t sched_data[768]; // takes us to 1024 bytes

    uint8_t reserved2[3072]; // takes us to 4096 bytes
} PerCPUState;

static_assert_sizeof(PerCPUState, ==, 4096);

// Assumes GS is already swapped to KernelGSBase...
static inline PerCPUState *state_get_per_cpu(void) {
    PerCPUState *ptr;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(ptr));
    return ptr;
}

#endif //__ANOS_SMP_STATE_H
