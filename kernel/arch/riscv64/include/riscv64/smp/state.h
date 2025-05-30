/*
 * stage3 - SMP per-CPU state (x86_64 specifics)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_SMP_ARCH_RISCV_STATE_H
#define __ANOS_SMP_ARCH_RISCV_STATE_H

#include "smp/state.h"

static inline PerCPUState *state_get_for_this_cpu(void) {
    PerCPUState *cpu_state;
    __asm__ volatile("mv %0, tp" : "=r"(cpu_state));
    return cpu_state;
}

#endif //__ANOS_SMP_ARCH_RISCV_STATE_H