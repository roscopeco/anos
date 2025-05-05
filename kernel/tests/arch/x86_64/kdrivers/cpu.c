#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "x86_64/kdrivers/cpu.h"

// just stub out CPUID, it's a PITA to build hosted (e.g. 32-bit relocs on macho64)
void init_cpuid(void) { /* nothing */ }

bool cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
           uint32_t *edx) {
    return false;
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

static MunitTest tests[] = {
        {"/rdseed64", test_rdseed64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdseed32", test_rdseed32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdrand64", test_rdrand64, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/rdrand32", test_rdrand32, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/x86_64/kdrivers/cpu", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *const argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}