/*
* Tests for kernel CPU driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "x86_64/kdrivers/cpu.h"

// just reimplement CPUID in a basic way - our ASM one is a PITA to build hosted
// (e.g. 32-bit relocs on macho64)
void init_cpuid(void) { /* nothing */ }

bool cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
           uint32_t *edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(0));

    return true;
}

static MunitResult test_rdseed64(const MunitParameter params[], void *data) {
    uint64_t val1 = 0, val2 = 0;

    munit_assert_true(cpu_rdseed64(&val1));
    munit_assert_true(cpu_rdseed64(&val2));

    munit_assert_uint64(val1, !=, val2);
    return MUNIT_OK;
}

static MunitResult test_rdseed32(const MunitParameter params[], void *data) {
    uint32_t val1 = 0, val2 = 0;

    munit_assert_true(cpu_rdseed32(&val1));
    munit_assert_true(cpu_rdseed32(&val2));

    munit_assert_uint64(val1, !=, val2);
    return MUNIT_OK;
}

static MunitResult test_rdrand64(const MunitParameter params[], void *data) {
    uint64_t val1 = 0, val2 = 0;

    munit_assert_true(cpu_rdrand64(&val1));
    munit_assert_true(cpu_rdrand64(&val2));

    munit_assert_uint64(val1, !=, val2);
    return MUNIT_OK;
}

static MunitResult test_rdrand32(const MunitParameter params[], void *data) {
    uint32_t val1 = 0, val2 = 0;

    munit_assert_true(cpu_rdrand32(&val1));
    munit_assert_true(cpu_rdrand32(&val2));

    munit_assert_uint64(val1, !=, val2);
    return MUNIT_OK;
}

static MunitTest tests_all[] = {
        {"/rdseed64", test_rdseed64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdseed32", test_rdseed32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdrand64", test_rdrand64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdrand32", test_rdrand32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static MunitTest tests_rdseed_only[] = {
        {"/rdseed64", test_rdseed64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdseed32", test_rdseed32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static MunitTest tests_rdrand_only[] = {
        {"/rdrand64", test_rdrand64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdrand32", test_rdrand32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite_all = {"/x86_64/kdrivers/cpu", tests_all, NULL, 1,
                                     MUNIT_SUITE_OPTION_NONE};

static const MunitSuite suite_rdseed_only = {"/x86_64/kdrivers/cpu",
                                             tests_rdseed_only, NULL, 1,
                                             MUNIT_SUITE_OPTION_NONE};

static const MunitSuite suite_rdrand_only = {"/x86_64/kdrivers/cpu",
                                             tests_rdrand_only, NULL, 1,
                                             MUNIT_SUITE_OPTION_NONE};

static bool cpu_supports_rdrand(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (ecx & (1 << 30)) != 0; // Bit 30 of ECX: RDRAND
}

static bool cpu_supports_rdseed(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(7, &eax, &ebx, &ecx, &edx);
    return (ebx & (1 << 18)) != 0; // Bit 18 of EBX: RDSEED
}

int main(const int argc, char *const argv[]) {
    if (cpu_supports_rdseed()) {
        if (cpu_supports_rdrand()) {
            return munit_suite_main(&suite_all, NULL, argc, argv);
        }

        return munit_suite_main(&suite_rdseed_only, NULL, argc, argv);
    }

    if (cpu_supports_rdrand()) {
        return munit_suite_main(&suite_rdrand_only, NULL, argc, argv);
    }

    return 0;
}