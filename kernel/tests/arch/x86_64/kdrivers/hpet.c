/*
 * Tests for kernel HPET driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "mock_recursive.h"

#include "mock_acpitables.h"
#include "mock_vmm.h"
#include "x86_64/kdrivers/hpet.h"

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

    // No pages were mapped (because there were no HPETs found...)
    munit_assert_uint(mock_vmm_get_total_page_maps(), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_hpet_period(const MunitParameter params[], void *data) {
    uint64_t test_caps;

    // Basic value test
    test_caps = 0xABCD000000000000;
    munit_assert_uint32(hpet_period(test_caps), ==, 0xABCD0000);

    // Max value
    test_caps = 0xFFFFFFFF00000000;
    munit_assert_uint32(hpet_period(test_caps), ==, 0xFFFFFFFF);

    // Zero value
    test_caps = 0x0000000000000000;
    munit_assert_uint32(hpet_period(test_caps), ==, 0x00000000);

    // Alternating pattern
    test_caps = 0xAAAAAAAA00000000;
    munit_assert_uint32(hpet_period(test_caps), ==, 0xAAAAAAAA);

    // Lower bits should not affect result
    test_caps = 0xABCD0000FFFFFFFF;
    munit_assert_uint32(hpet_period(test_caps), ==, 0xABCD0000);

    return MUNIT_OK;
}

static MunitResult test_hpet_vendor(const MunitParameter params[], void *data) {
    uint64_t test_caps;

    // Basic value
    test_caps = 0x0000000012340000;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0x1234);

    // Max value
    test_caps = 0x00000000FFFF0000;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0xFFFF);

    // Min value
    test_caps = 0x0000000000000000;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0x0000);

    // Boundary values
    test_caps = 0x0000000000010000;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0x0001);

    test_caps = 0x00000000FFFE0000;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0xFFFE);

    // Surrounding bits should not affect result
    test_caps = 0xFFFFFFFF0000FFFF;
    munit_assert_uint16(hpet_vendor(test_caps), ==, 0x0000);

    return MUNIT_OK;
}

static MunitResult test_hpet_timer_count(const MunitParameter params[],
                                         void *data) {
    uint64_t test_caps;

    // Max value (31 + 1 = 32 timers)
    test_caps = 0x0000000000001F00;
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 32);

    // Min value (0 + 1 = 1 timer)
    test_caps = 0x0000000000000000;
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 1);

    // Mid value (15 + 1 = 16 timers)
    test_caps = 0x0000000000000F00;
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 16);

    // Test various counts in range
    test_caps = 0x0000000000000100; // 2 timers
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 2);

    test_caps = 0x0000000000001000; // 17 timers
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 17);

    // Surrounding bits should not affect result
    test_caps = 0xFFFFFFFFFFFF0FFF;
    munit_assert_uint8(hpet_timer_count(test_caps), ==, 16);

    return MUNIT_OK;
}

static MunitResult test_hpet_is_64_bit(const MunitParameter params[],
                                       void *data) {
    uint64_t test_caps;

    // Basic true case
    test_caps = 0x0000000000002000;
    munit_assert_true(hpet_is_64_bit(test_caps));

    // Basic false case
    test_caps = 0x0000000000000000;
    munit_assert_false(hpet_is_64_bit(test_caps));

    // Only target bit clear in otherwise set field
    test_caps = 0xFFFFFFFFFFFFDFFF;
    munit_assert_false(hpet_is_64_bit(test_caps));

    return MUNIT_OK;
}

static MunitResult test_hpet_can_legacy(const MunitParameter params[],
                                        void *data) {
    uint64_t test_caps;

    // Basic true case
    test_caps = 0x0000000000008000;
    munit_assert_true(hpet_can_legacy(test_caps));

    // Basic false case
    test_caps = 0x0000000000000000;
    munit_assert_false(hpet_can_legacy(test_caps));

    // All bits set except target bit
    test_caps = 0xFFFFFFFFFFFF7FFF;
    munit_assert_false(hpet_can_legacy(test_caps));

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    return NULL;
}

static void teardown(void *param) {
    mock_acpitables_reset();
    mock_vmm_reset();
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init", test_init_null, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {"/hpet_period", test_hpet_period, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/hpet_vendor", test_hpet_vendor, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/hpet_timer_count", test_hpet_timer_count, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/hpet_is_64_bit", test_hpet_is_64_bit, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/hpet_can_legacy", test_hpet_can_legacy, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/kdrivers/hpet",
                                      test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
