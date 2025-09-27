/*
 * stage3 - ISR stack frame (x86_64)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_ISR_FRAME_H
#define __ANOS_KERNEL_ARCH_X86_64_ISR_FRAME_H

#include <stdint.h>

/*
 * N.B. This must be kept in-step with the pusha_sysv macros in isr_common!
 */
typedef struct {
    uintptr_t rbp;
    uintptr_t r11;
    uintptr_t r10;
    uintptr_t r9;
    uintptr_t r8;
    uintptr_t rdi;
    uintptr_t rsi;
    uintptr_t rdx;
    uintptr_t rcx;
    uintptr_t rax;
} Registers;

typedef struct {
    Registers registers;
    uintptr_t return_address;
    uintptr_t cs;
    uintptr_t rflags;
    uintptr_t rsp;
} IsrStackFrameNoCode;

typedef struct {
    Registers registers;
    uintptr_t code;
    uintptr_t return_address;
    uintptr_t cs;
    uintptr_t rflags;
    uintptr_t rsp;
} IsrStackFrameWithCode;

#endif
