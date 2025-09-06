/*
 * Tests for printf module
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "munit.h"
#include "printf.h"

// Test buffer for sprintf tests
static char test_buffer[1024];

// Custom output function for fctprintf tests
static char fct_buffer[1024];
static size_t fct_pos = 0;

static void fct_output(char character, void *arg) {
    (void)arg;
    if (fct_pos < sizeof(fct_buffer) - 1) {
        fct_buffer[fct_pos++] = character;
        fct_buffer[fct_pos] = '\0';
    }
}

static void reset_fct_buffer(void) {
    memset(fct_buffer, 0, sizeof(fct_buffer));
    fct_pos = 0;
}

// Test setup
static void *setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    memset(test_buffer, 0, sizeof(test_buffer));
    reset_fct_buffer();

    return NULL;
}

// ============================================================================
// SPRINTF BASIC TESTS
// ============================================================================

static MunitResult test_sprintf_string(const MunitParameter params[],
                                       void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Hello, %s!", "World");

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(test_buffer, "Hello, World!");

    return MUNIT_OK;
}

static MunitResult test_sprintf_integer(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Number: %d", 42);

    munit_assert_int(result, ==, 10);
    munit_assert_string_equal(test_buffer, "Number: 42");

    return MUNIT_OK;
}

static MunitResult test_sprintf_negative_integer(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Negative: %d", -123);

    munit_assert_int(result, ==, 14);
    munit_assert_string_equal(test_buffer, "Negative: -123");

    return MUNIT_OK;
}

static MunitResult test_sprintf_unsigned_integer(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Unsigned: %u", 4294967295U);

    munit_assert_int(result, ==, 20);
    munit_assert_string_equal(test_buffer, "Unsigned: 4294967295");

    return MUNIT_OK;
}

static MunitResult test_sprintf_hex_lowercase(const MunitParameter params[],
                                              void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Hex: 0x%x", 255);

    munit_assert_int(result, ==, 9);
    munit_assert_string_equal(test_buffer, "Hex: 0xff");

    return MUNIT_OK;
}

static MunitResult test_sprintf_hex_uppercase(const MunitParameter params[],
                                              void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Hex: 0x%X", 255);

    munit_assert_int(result, ==, 9);
    munit_assert_string_equal(test_buffer, "Hex: 0xFF");

    return MUNIT_OK;
}

static MunitResult test_sprintf_octal(const MunitParameter params[],
                                      void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Octal: %o", 64);

    munit_assert_int(result, ==, 10);
    munit_assert_string_equal(test_buffer, "Octal: 100");

    return MUNIT_OK;
}

static MunitResult test_sprintf_pointer(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    void *ptr = (void *)0x12345678;
    int result = sprintf_(test_buffer, "Pointer: %p", ptr);

    munit_assert_int(result, ==, 25);
    // Check that it contains the expected hex value (format may vary)
    munit_assert_ptr_not_null(strstr(test_buffer, "12345678"));

    return MUNIT_OK;
}

static MunitResult test_sprintf_character(const MunitParameter params[],
                                          void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Char: %c", 'A');

    munit_assert_int(result, ==, 7);
    munit_assert_string_equal(test_buffer, "Char: A");

    return MUNIT_OK;
}

static MunitResult test_sprintf_percent_literal(const MunitParameter params[],
                                                void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Percent: %%");

    munit_assert_int(result, ==, 10);
    munit_assert_string_equal(test_buffer, "Percent: %");

    return MUNIT_OK;
}

// ============================================================================
// SPRINTF WIDTH AND PADDING TESTS
// ============================================================================

static MunitResult test_sprintf_width_right_align(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Number: %5d", 42);

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(test_buffer, "Number:    42");

    return MUNIT_OK;
}

static MunitResult test_sprintf_width_left_align(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Number: %-5d", 42);

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(test_buffer, "Number: 42   ");

    return MUNIT_OK;
}

static MunitResult test_sprintf_width_zero_pad(const MunitParameter params[],
                                               void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Number: %05d", 42);

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(test_buffer, "Number: 00042");

    return MUNIT_OK;
}

static MunitResult
test_sprintf_width_hex_zero_pad(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Hex: 0x%08x", 0xABCD);

    munit_assert_int(result, ==, 15);
    munit_assert_string_equal(test_buffer, "Hex: 0x0000abcd");

    return MUNIT_OK;
}

// ============================================================================
// SPRINTF PRECISION TESTS
// ============================================================================

static MunitResult test_sprintf_string_precision(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "String: %.5s", "Hello, World!");

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(test_buffer, "String: Hello");

    return MUNIT_OK;
}

static MunitResult test_sprintf_integer_precision(const MunitParameter params[],
                                                  void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Number: %.6d", 42);

    munit_assert_int(result, ==, 14);
    munit_assert_string_equal(test_buffer, "Number: 000042");

    return MUNIT_OK;
}

// ============================================================================
// SNPRINTF TESTS (BUFFER LIMITING)
// ============================================================================

static MunitResult test_snprintf_exact_fit(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    char small_buffer[6]; // "Hello" + null terminator
    int result = snprintf_(small_buffer, sizeof(small_buffer), "Hello");

    munit_assert_int(result, ==, 5);
    munit_assert_string_equal(small_buffer, "Hello");

    return MUNIT_OK;
}

static MunitResult test_snprintf_truncation(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    char small_buffer[6]; // Only 5 chars + null terminator
    int result = snprintf_(small_buffer, sizeof(small_buffer), "Hello, World!");

    munit_assert_int(result, ==, 13); // Returns what it would have written
    munit_assert_string_equal(small_buffer, "Hello");

    return MUNIT_OK;
}

static MunitResult test_snprintf_zero_size(const MunitParameter params[],
                                           void *data) {
    (void)params;
    (void)data;

    char dummy_buffer[1];
    int result = snprintf_(dummy_buffer, 0, "Hello");

    munit_assert_int(result, ==, 5);
    // Buffer should be unchanged since size is 0

    return MUNIT_OK;
}

// ============================================================================
// FCTPRINTF TESTS (CUSTOM OUTPUT FUNCTION)
// ============================================================================

static MunitResult test_fctprintf_basic(const MunitParameter params[],
                                        void *data) {
    (void)params;
    (void)data;

    int result = fctprintf(fct_output, NULL, "Hello, %s!", "World");

    munit_assert_int(result, ==, 13);
    munit_assert_string_equal(fct_buffer, "Hello, World!");

    return MUNIT_OK;
}

static MunitResult test_fctprintf_multiple_calls(const MunitParameter params[],
                                                 void *data) {
    (void)params;
    (void)data;

    fctprintf(fct_output, NULL, "First: %d", 1);
    fctprintf(fct_output, NULL, ", Second: %d", 2);

    munit_assert_string_equal(fct_buffer, "First: 1, Second: 2");

    return MUNIT_OK;
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

static MunitResult test_sprintf_null_string(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "String: %s", (char *)NULL);

    munit_assert_int(result, ==, 14);
    munit_assert_string_equal(test_buffer, "String: (null)");

    return MUNIT_OK;
}

static MunitResult test_sprintf_empty_format(const MunitParameter params[],
                                             void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "");

    munit_assert_int(result, ==, 0);
    munit_assert_string_equal(test_buffer, "");

    return MUNIT_OK;
}

static MunitResult
test_sprintf_no_format_specifiers(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Just a plain string");

    munit_assert_int(result, ==, 19);
    munit_assert_string_equal(test_buffer, "Just a plain string");

    return MUNIT_OK;
}

static MunitResult test_sprintf_long_integers(const MunitParameter params[],
                                              void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Long: %ld", 1234567890L);

    munit_assert_int(result, ==, 16);
    munit_assert_string_equal(test_buffer, "Long: 1234567890");

    return MUNIT_OK;
}

static MunitResult
test_sprintf_multiple_specifiers(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    int result = sprintf_(test_buffer, "Int: %d, Hex: %x, String: %s", 42, 255,
                          "test");

    munit_assert_int(result, ==, 30);
    munit_assert_string_equal(test_buffer, "Int: 42, Hex: ff, String: test");

    return MUNIT_OK;
}

// Test array
static MunitTest test_suite_tests[] = {
        // Basic sprintf tests
        {(char *)"/printf/sprintf_string", test_sprintf_string, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_integer", test_sprintf_integer, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_negative_integer",
         test_sprintf_negative_integer, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_unsigned_integer",
         test_sprintf_unsigned_integer, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_hex_lowercase", test_sprintf_hex_lowercase,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_hex_uppercase", test_sprintf_hex_uppercase,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_octal", test_sprintf_octal, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_pointer", test_sprintf_pointer, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_character", test_sprintf_character, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_percent_literal",
         test_sprintf_percent_literal, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        // Width and padding tests
        {(char *)"/printf/sprintf_width_right_align",
         test_sprintf_width_right_align, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_width_left_align",
         test_sprintf_width_left_align, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_width_zero_pad", test_sprintf_width_zero_pad,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_width_hex_zero_pad",
         test_sprintf_width_hex_zero_pad, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        // Precision tests
        {(char *)"/printf/sprintf_string_precision",
         test_sprintf_string_precision, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_integer_precision",
         test_sprintf_integer_precision, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        // snprintf tests
        {(char *)"/printf/snprintf_exact_fit", test_snprintf_exact_fit, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/snprintf_truncation", test_snprintf_truncation, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/snprintf_zero_size", test_snprintf_zero_size, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        // fctprintf tests
        {(char *)"/printf/fctprintf_basic", test_fctprintf_basic, setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/fctprintf_multiple_calls",
         test_fctprintf_multiple_calls, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        // Edge case tests
        {(char *)"/printf/sprintf_null_string", test_sprintf_null_string, setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_empty_format", test_sprintf_empty_format,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_no_format_specifiers",
         test_sprintf_no_format_specifiers, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/printf/sprintf_long_integers", test_sprintf_long_integers,
         setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/printf/sprintf_multiple_specifiers",
         test_sprintf_multiple_specifiers, setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"printf", argc, argv);
}