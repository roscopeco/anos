/*
 * Abuse Throttling Utilities (x86_64)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Provides spin-based delay mechanisms for deterring brute-force
 * or abusive access patterns (e.g. invalid capability guesses) without
 * engaging the scheduler or introducing sleep-based side effects.
 *
 * Can be used in any path where we want to punish repeat offenders
 * without impacting legitimate users.
 * 
 * Also introduces jitter to mitigate potential timing attacks.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_THROTTLE_H
#define __ANOS_KERNEL_ARCH_X86_64_THROTTLE_H

#include <stdint.h>

#include "process.h"
#include "x86_64/kdrivers/cpu.h"

static inline void spin_delay_cycles(uint64_t cycles) {
    uint64_t start = cpu_read_tsc();
    while ((cpu_read_tsc() - start) < cycles) {
        __asm__ volatile("pause");
    }
}

static inline uint64_t rand_entropy(void) {
    uint64_t value;

    if (cpu_rdrand64(&value)) {
        return value;
    }

    value = cpu_read_tsc();
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

/*
 * Apply escalating, randomised spin delay on abuse.
 * 
 * Delay escalates based on per-process failure count.
 */
static inline void throttle_abuse(Process *proc) {
    uint64_t base = 50000 + (proc->cap_failures * 5000);
    if (base > 1000000)
        base = 1000000;

    const uint64_t jitter = rand_entropy() % base;
    const uint64_t delay = base + jitter;

    spin_delay_cycles(delay);
    proc->cap_failures++;
}

#ifdef ENABLE_SYSCALL_THROTTLE_RESET
// Call this after a successful access to reset penalty
static inline void throttle_reset(Process *proc) { proc->cap_failures = 0; }
#endif

#endif // __ANOS_KERNEL_THROTTLE_H
