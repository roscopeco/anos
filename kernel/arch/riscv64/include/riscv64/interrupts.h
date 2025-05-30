/*
 * stage3 - Interrupt support for RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_INTERRUPTS_H
#define __ANOS_KERNEL_ARCH_RISCV64_INTERRUPTS_H

#include <stdint.h>

typedef void (*isr_dispatcher)(void);

static inline void set_supervisor_trap_vector(isr_dispatcher dispatcher) {
    uintptr_t addr = (uintptr_t)dispatcher;
    __asm__ volatile("csrw stvec, %0" ::"r"(addr));
}

void install_ivt_entry(uint8_t entry, isr_dispatcher dispatcher);

void install_interrupts(void);

#endif
