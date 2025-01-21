/*
 * Tests for kernel driver interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "kdrivers/drivers.h"
#include "mock_kernel_drivers.h"
#include "munit.h"

static const ACPI_RSDT valid_rsdt = {.header = {
                                             .checksum = 0x123,
                                             .length = sizeof(ACPI_RSDT),
                                     }};

static MunitResult test_init_null(const MunitParameter params[], void *param) {
    bool result = kernel_drivers_init(NULL);

    munit_assert_false(result);
    munit_assert_uint(mock_kernel_drivers_get_hpet_init_call_count(), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_init_valid(const MunitParameter params[], void *param) {
    bool result = kernel_drivers_init(&valid_rsdt);

    munit_assert_true(result);
    munit_assert_uint(mock_kernel_drivers_get_hpet_init_call_count(), ==, 1);
    munit_assert_ptr(mock_kernel_drivers_get_last_hpet_init_rsdt(), ==,
                     &valid_rsdt);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    return NULL;
}

static void teardown(void *param) { mock_kernel_drivers_reset(); }

static MunitTest test_suite_tests[] = {
        {(char *)"/init_null", test_init_null, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_valid", test_init_valid, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/kdrivers", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
