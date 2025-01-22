/*
 * Tests for kernel HPET driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "kdrivers/hpet.h"
#include "mock_acpitables.h"
#include "munit.h"

static ACPI_RSDT valid_rsdt = {.header = {
                                       .checksum = 0x23,
                                       .length = sizeof(ACPI_RSDT),
                               }};

static MunitResult test_init_null(const MunitParameter params[], void *param) {
    bool result = hpet_init(NULL);

    munit_assert_false(result);
    munit_assert_uint(mock_acpitables_get_acpi_tables_find_call_count(), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_init_valid(const MunitParameter params[], void *param) {
    bool result = hpet_init(&valid_rsdt);

    // Result true, we _did_ initialize all zero HPETs...
    munit_assert_true(result);
    munit_assert_uint(mock_acpitables_get_acpi_tables_find_call_count(), ==, 1);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    return NULL;
}

static void teardown(void *param) { mock_acpitables_reset(); }

static MunitTest test_suite_tests[] = {
        {(char *)"/init", test_init_null, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/kdrivers/hpet",
                                      test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
