/*
 * Tests for process utility functions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "munit.h"

#define VM_PAGE_SIZE ((0x1000))

// Forward declarations of functions to test
extern unsigned int round_up_to_page_size(const size_t size);
extern unsigned int round_up_to_machine_word_size(const size_t size);

// Test setup
static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;
    return NULL;
}

// ============================================================================
// PAGE SIZE ROUNDING TESTS
// ============================================================================

static MunitResult
test_round_up_to_page_size_aligned(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Test with already page-aligned size
    unsigned int result = round_up_to_page_size(VM_PAGE_SIZE);
    munit_assert_uint32(result, ==, VM_PAGE_SIZE);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_page_size_unaligned(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    // Test with unaligned size that needs rounding up
    unsigned int result = round_up_to_page_size(VM_PAGE_SIZE + 1);
    munit_assert_uint32(result, ==, VM_PAGE_SIZE * 2);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_page_size_zero(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Test with zero size
    unsigned int result = round_up_to_page_size(0);
    munit_assert_uint32(result, ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_page_size_small(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    // Test with small size less than page size
    unsigned int result = round_up_to_page_size(100);
    munit_assert_uint32(result, ==, VM_PAGE_SIZE);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_page_size_multiple_pages(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    // Test with size requiring multiple pages
    unsigned int result = round_up_to_page_size(VM_PAGE_SIZE * 3 + 500);
    munit_assert_uint32(result, ==, VM_PAGE_SIZE * 4);

    return MUNIT_OK;
}

// ============================================================================
// MACHINE WORD SIZE ROUNDING TESTS
// ============================================================================

static MunitResult
test_round_up_to_machine_word_size_aligned(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    // Test with already word-aligned size
    unsigned int result = round_up_to_machine_word_size(sizeof(uintptr_t));
    munit_assert_uint32(result, ==, sizeof(uintptr_t));

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_machine_word_size_unaligned(const MunitParameter params[],
                                             void *data) {
    (void)params;
    (void)data;

    // Test with unaligned size that needs rounding up
    unsigned int result = round_up_to_machine_word_size(sizeof(uintptr_t) + 1);
    munit_assert_uint32(result, ==, sizeof(uintptr_t) * 2);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_machine_word_size_zero(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    // Test with zero size
    unsigned int result = round_up_to_machine_word_size(0);
    munit_assert_uint32(result, ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_machine_word_size_small(const MunitParameter params[],
                                         void *data) {
    (void)params;
    (void)data;

    // Test with small size less than word size
    unsigned int result = round_up_to_machine_word_size(3);
    munit_assert_uint32(result, ==, sizeof(uintptr_t));

    return MUNIT_OK;
}

static MunitResult
test_round_up_to_machine_word_size_multiple_words(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    // Test with size requiring multiple words
    unsigned int result =
            round_up_to_machine_word_size(sizeof(uintptr_t) * 2 + 3);
    munit_assert_uint32(result, ==, sizeof(uintptr_t) * 3);

    return MUNIT_OK;
}

// Test array
static MunitTest test_suite_tests[] = {
        // Page size rounding tests
        {(char *)"/process_utils/round_up_to_page_size_aligned",
         test_round_up_to_page_size_aligned, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_page_size_unaligned",
         test_round_up_to_page_size_unaligned, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_page_size_zero",
         test_round_up_to_page_size_zero, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/process_utils/round_up_to_page_size_small",
         test_round_up_to_page_size_small, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/process_utils/round_up_to_page_size_multiple_pages",
         test_round_up_to_page_size_multiple_pages, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        // Machine word size rounding tests
        {(char *)"/process_utils/round_up_to_machine_word_size_aligned",
         test_round_up_to_machine_word_size_aligned, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_machine_word_size_unaligned",
         test_round_up_to_machine_word_size_unaligned, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_machine_word_size_zero",
         test_round_up_to_machine_word_size_zero, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_machine_word_size_small",
         test_round_up_to_machine_word_size_small, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/process_utils/round_up_to_machine_word_size_multiple_words",
         test_round_up_to_machine_word_size_multiple_words, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"process_utils", argc, argv);
}