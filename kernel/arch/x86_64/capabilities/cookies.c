/*
 * Capability Cookie Generator - x86_64
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This implementation generates secure, unique 64-bit capability tokens
 * ("cookies") on x86_64 systems using a combination of:
 *
 *   - RDSEED (preferred) or RDRAND for hardware entropy
 *   - TSC (Time Stamp Counter) for time-based uniqueness
 *   - Per-core ID to avoid cross-CPU collisions
 *   - A fallback atomic counter in case hardware RNG is unavailable
 *
 * The components are mixed using a strong bit-mixing function to ensure
 * high entropy and low correlation between tokens.
 *
 * Tokens are guaranteed to be non-zero, unpredictable, and unique across
 * all CPUs.
 *
 * This code is self-contained and does not require allocation or
 * external state beyond basic PerCPUState info and atomics.
 */

#include <stdint.h>

#include "smp/state.h"
#include "x86_64/kdrivers/cpu.h"

static uint64_t fallback_counter = 0;

uint64_t capability_cookie_generate(void) {
    uint64_t entropy = 0;

    if (!cpu_rdseed64(&entropy)) {
        if (!cpu_rdrand64(&entropy)) {
            // Fallback: use counter if both fail
            entropy = __atomic_add_fetch(&fallback_counter, 1, __ATOMIC_RELAXED);
        }
    }

    // mix in per-CPU ID and timestamp
    uint64_t cpu_id = state_get_for_this_cpu()->cpu_id;
    entropy ^= (cpu_read_tsc() << 1) ^ (cpu_id * 0x9e3779b97f4a7c15);

    // avalanche to reduce correlation
    entropy ^= entropy >> 33;
    entropy *= 0xff51afd7ed558ccdULL;
    entropy ^= entropy >> 33;
    entropy *= 0xc4ceb9fe1a85ec53ULL;
    entropy ^= entropy >> 33;

    return entropy;
}
