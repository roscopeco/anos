#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "capabilities/cookies.h"
#include "smp/state.h"

#ifdef __x86_64__
#include "x86_64/kdrivers/cpu.h"

// just stub out CPUID, it's a PITA to build hosted (e.g. 32-bit relocs on macho64)
// and we don't need it here anyway...
void init_cpuid(void) { /* nothing */ }

bool cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
           uint32_t *edx) {
    return false;
}

PerCPUState ___test_cpu_state;

void set_fake_cpu_id(uint64_t core) { ___test_cpu_state.cpu_id = core; }
#endif

static MunitResult
test_capability_cookie_generate(const MunitParameter params[], void *data) {
    enum { NUM_COOKIES = 100 };
    uint64_t cookies[NUM_COOKIES] = {0};

    for (int i = 0; i < NUM_COOKIES; i++) {
        cookies[i] = capability_cookie_generate();
        munit_assert_not_null((void *)(uintptr_t)cookies[i]);
    }

    // Basic uniqueness check
    for (int i = 0; i < NUM_COOKIES; i++) {
        for (int j = i + 1; j < NUM_COOKIES; j++) {
            munit_assert_uint64(cookies[i], !=, cookies[j]);
        }
    }

    return MUNIT_OK;
}

#include "munit.h"
#include <stdbool.h>
#include <stdint.h>

#define NUM_COOKIES 100000
#define HISTO_BUCKETS 256

static MunitResult test_cookie_soak_histogram(const MunitParameter params[],
                                              void *data) {
    uint64_t cookies[NUM_COOKIES] = {0};
    uint8_t histogram[HISTO_BUCKETS] = {0};

    for (size_t i = 0; i < NUM_COOKIES; i++) {
        cookies[i] = capability_cookie_generate();
        munit_assert_not_null((void *)(uintptr_t)cookies[i]);

        uint8_t top_byte = cookies[i] >> 56;
        histogram[top_byte]++;
    }

    // Check uniqueness (basic brute force for small N)
    for (size_t i = 0; i < NUM_COOKIES; i++) {
        for (size_t j = i + 1; j < NUM_COOKIES; j++) {
            munit_assert(cookies[i] != cookies[j]);
        }
    }

    // sanity check histogram isn't all in 1 bin
    int active_bins = 0;
    for (int i = 0; i < HISTO_BUCKETS; i++) {
        if (histogram[i] > 0)
            active_bins++;
    }
    munit_assert(active_bins >
                 8); // Expect at least ~8 different top-byte values

    return MUNIT_OK;
}

static MunitResult test_cookie_cross_core(const MunitParameter params[],
                                          void *data) {
    enum { CORES = MAX_CPU_COUNT, PER_CORE = 1000 };
    uint64_t cookies[CORES][PER_CORE] = {{0}};

    for (int core = 0; core < CORES; core++) {
        set_fake_cpu_id(core); // simulate hart/core
        for (int i = 0; i < PER_CORE; i++) {
            cookies[core][i] = capability_cookie_generate();
            munit_assert_not_null((void *)(uintptr_t)cookies[core][i]);
        }
    }

    // Check inter-core uniqueness
    for (int c1 = 0; c1 < CORES; c1++) {
        for (int i = 0; i < PER_CORE; i++) {
            for (int c2 = c1; c2 < CORES; c2++) {
                for (int j = (c1 == c2 ? i + 1 : 0); j < PER_CORE; j++) {
                    munit_assert(cookies[c1][i] != cookies[c2][j]);
                }
            }
        }
    }

    return MUNIT_OK;
}
static MunitTest tests[] = {
        {"/generate", test_capability_cookie_generate, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/soak", test_cookie_soak_histogram, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/cross_core", test_cookie_cross_core, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/capability_cookies", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *const argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
