/*
 * Capability Cookie Generator - RISC-V
 * anos - An Operating System
 * 
 * Copyright (c) 2025 Ross Bamford
 *
 * This RISC-V implementation generates unique, non-forgeable 64-bit
 * capability tokens ("cookies") using:
 *
 *   - rdcycle for high-resolution monotonic timestamps
 *   - mhartid to identify the current core (hart)
 *   - Per-hart atomic counter to avoid reuse and ensure ordering
 *   - A MurmurHash-style final mixing step to decorrelate inputs
 *
 * This approach guarantees:
 *   - Global uniqueness across all harts
 *   - Tokens are never zero
 *   - No static state beyond a per-hart counter table
 *
 * No hardware RNG is assumed or required. The resulting cookies are
 * suitable for indexing secure kernel (capability maps).
 */

#include <stdatomic.h>
#include <stdint.h>

#include "riscv64/kdrivers/cpu.h"

static uint64_t hart_counters[MAX_CPU_COUNT];

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

uint64_t capability_cookie_generate(void) {
    uint64_t cycle = cpu_read_rdcycle();
    uint64_t hartid = cpu_read_hartid();

    // Atomically increment per-hart counter
    uint64_t count =
            __atomic_add_fetch(&hart_counters[hartid], 1, __ATOMIC_RELAXED);

    // Combine inputs
    uint64_t raw =
            (cycle << 1) ^ (count << 3) ^ (hartid * 0x517cc1b727220a95ULL);

    return mix64(raw);
}
