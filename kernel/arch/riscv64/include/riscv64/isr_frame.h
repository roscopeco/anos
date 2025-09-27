/*
 * stage3 - ISR stack frame (riscv64)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_ISR_FRAME_H
#define __ANOS_KERNEL_ARCH_X86_64_ISR_FRAME_H

#include <stdint.h>

/*
 * N.B. This must be kept in-step with the _trap_dispatcher_impl macro in isr_dispatch.S
 */
typedef struct {
    uintptr_t ft11;
    uintptr_t ft10;
    uintptr_t ft9;
    uintptr_t ft8;
    uintptr_t fa7;
    uintptr_t fa6;
    uintptr_t fa5;
    uintptr_t fa4;
    uintptr_t fa3;
    uintptr_t fa2;
    uintptr_t fa1;
    uintptr_t fa0;
    uintptr_t ft7;
    uintptr_t ft6;
    uintptr_t ft5;
    uintptr_t ft4;
    uintptr_t ft3;
    uintptr_t ft2;
    uintptr_t ft1;
    uintptr_t ft0;
    uintptr_t res;
    uintptr_t t6;
    uintptr_t t5;
    uintptr_t t4;
    uintptr_t t3;
    uintptr_t a7;
    uintptr_t a6;
    uintptr_t a5;
    uintptr_t a4;
    uintptr_t a3;
    uintptr_t a2;
    uintptr_t a1;
    uintptr_t a0;
    uintptr_t t2;
    uintptr_t t1;
    uintptr_t fcsr;
    uintptr_t t0;
    uintptr_t ra;
    uintptr_t tp;
    uintptr_t rbp; // NOTE: Not actually a base pointer on RISC-V! Actually just a reserved stack slot...
} Registers;

typedef struct {
    Registers registers;

    // TODO code/no-code is probably useless on RISC-V...
} IsrStackFrameNoCode;

typedef struct {
    Registers registers;

    // TODO code/no-code is probably useless on RISC-V...
} IsrStackFrameWithCode;

#endif
