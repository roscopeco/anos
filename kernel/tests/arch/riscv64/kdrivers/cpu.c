#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "riscv64/kdrivers/cpu.h"

static MunitResult test_rdcycle(const MunitParameter params[], void *data) {
    uint64_t a = cpu_read_rdcycle();
    uint64_t b = cpu_read_rdcycle();

    munit_assert_true(b >= a);

    return MUNIT_OK;
}

static MunitResult test_rdtime(const MunitParameter params[], void *data) {
    uint64_t a = cpu_read_rdtime();
    uint64_t b = cpu_read_rdtime();

    munit_assert_true(b >= a);

    return MUNIT_OK;
}

static MunitResult test_mhartid(const MunitParameter params[], void *data) {
    uint64_t hartid = cpu_read_mhartid();

    munit_assert_true(hartid < MAX_CPU_COUNT);

    return MUNIT_OK;
}

static MunitTest tests[] = {{"/rdcycle", test_rdcycle, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {"/rdtime", test_rdtime, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {"/mhartid", test_mhartid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                            {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/riscv64/kdrivers/cpu", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *const argv[]) { return munit_suite_main(&suite, NULL, argc, argv); }