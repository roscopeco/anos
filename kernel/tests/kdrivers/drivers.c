/*
 * Tests for kernel driver interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "kdrivers/drivers.h"
#include "mock_kernel_drivers.h"
#include "munit.h"

static ACPI_RSDT valid_rsdt = {.header = {
                                       .checksum = (uint8_t)0x123,
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

static MunitResult test_alloc_0(const MunitParameter params[], void *param) {
    void *result = kernel_drivers_alloc_pages(0);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_alloc_1(const MunitParameter params[], void *param) {
    void *result = kernel_drivers_alloc_pages(1);

    munit_assert_ptr(result, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    return MUNIT_OK;
}

static MunitResult test_alloc_248(const MunitParameter params[], void *param) {
    void *result = kernel_drivers_alloc_pages(248);
    munit_assert_ptr(result, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    return MUNIT_OK;
}

static MunitResult test_alloc_249(const MunitParameter params[], void *param) {
    void *result = kernel_drivers_alloc_pages(249);

    munit_assert_null(result);

    return MUNIT_OK;
}
static MunitResult test_alloc_1_1(const MunitParameter params[], void *param) {
    void *result1 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result1, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    void *result2 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result2, ==, (void *)KERNEL_DRIVER_VADDR_BASE + 0x1000);

    return MUNIT_OK;
}

static MunitResult test_alloc_1_247(const MunitParameter params[],
                                    void *param) {
    void *result1 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result1, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    void *result2 = kernel_drivers_alloc_pages(247);
    munit_assert_ptr(result2, ==, (void *)KERNEL_DRIVER_VADDR_BASE + 0x1000);

    return MUNIT_OK;
}

static MunitResult test_alloc_247_1(const MunitParameter params[],
                                    void *param) {
    void *result1 = kernel_drivers_alloc_pages(247);
    munit_assert_ptr(result1, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    void *result2 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result2, ==, (void *)KERNEL_DRIVER_VADDR_BASE + 0xf7000);

    return MUNIT_OK;
}

static MunitResult test_alloc_1_248(const MunitParameter params[],
                                    void *param) {
    void *result1 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result1, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    void *result2 = kernel_drivers_alloc_pages(248);
    munit_assert_null(result2);

    return MUNIT_OK;
}

static MunitResult test_alloc_248_1(const MunitParameter params[],
                                    void *param) {
    void *result1 = kernel_drivers_alloc_pages(1);
    munit_assert_ptr(result1, ==, (void *)KERNEL_DRIVER_VADDR_BASE);

    void *result2 = kernel_drivers_alloc_pages(248);
    munit_assert_null(result2);

    return MUNIT_OK;
}

static MunitResult test_alloc_1x1(const MunitParameter params[], void *param) {
    for (int i = 0; i < 248; i++) {
        void *result1 = kernel_drivers_alloc_pages(1);
        munit_assert_ptr(result1, ==,
                         ((void *)(KERNEL_DRIVER_VADDR_BASE + (i * 0x1000))));
    }

    void *result2 = kernel_drivers_alloc_pages(1);
    munit_assert_null(result2);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    return NULL;
}

void kernel_drivers_alloc_pages_reset(void);

static void teardown(void *param) {
    kernel_drivers_alloc_pages_reset();
    mock_kernel_drivers_reset();
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init_null", test_init_null, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_valid", test_init_valid, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc_0", test_alloc_0, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_1", test_alloc_1, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_248", test_alloc_248, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_249", test_alloc_249, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_1_1", test_alloc_1_1, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_1_247", test_alloc_1_247, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_247_1", test_alloc_247_1, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_1_248", test_alloc_1_248, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_248_1", test_alloc_248_1, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_1x1", test_alloc_1x1, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/kdrivers", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
