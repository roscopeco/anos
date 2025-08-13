/*
 * stage3 - SMP per-CPU state (x86_64 specifics)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_SMP_ARCH_X86_64_STATE_H
#define __ANOS_SMP_ARCH_X86_64_STATE_H

#include "smp/state.h"

#ifdef CONSERVATIVE_BUILD
#ifdef CONSERVATIVE_PANICKY
#include "panic.h"
#define cond_panic panic
#else
#include "kprintf.h"
#define cond_panic kprintf
#endif
#endif

// Assumes GS is already swapped to KernelGSBase...
static inline PerCPUState *state_get_for_this_cpu(void) {
    PerCPUState *ptr;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(ptr));

#ifdef CONSERVATIVE_BUILD
    if (((uintptr_t)ptr & 0xffffffff00000000) == 0) {
        cond_panic("state_get_for_this_cpu assertion failed: ptr not in kernel "
                   "space");
    }
#endif

    return ptr;
}

#endif //__ANOS_SMP_ARCH_X86_64_STATE_H