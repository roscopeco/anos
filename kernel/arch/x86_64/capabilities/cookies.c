#include <stdint.h>

#include "smp/state.h"
#include "x86_64/kdrivers/cpu.h"

static uint64_t fallback_counter = 0;

uint64_t capability_cookie_generate(void) {
    uint64_t entropy = 0;

    if (!cpu_rdseed64(&entropy)) {
        if (!cpu_rdrand64(&entropy)) {
            // Fallback: use counter if both fail
            entropy =
                    __atomic_add_fetch(&fallback_counter, 1, __ATOMIC_RELAXED);
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
