#include "munit.h"

#include <stdint.h>
#include <string.h>

#include "printdec.h"

// Test fixture to hold output buffer
typedef struct {
    char buffer[64];
    size_t pos;
} TestFixture;

// Global pointer for test fixture
static TestFixture *current_fixture;

// Output handler that writes to our buffer
static void capture_char(char c) {
    if (current_fixture->pos < sizeof(current_fixture->buffer) - 1) {
        current_fixture->buffer[current_fixture->pos++] = c;
        current_fixture->buffer[current_fixture->pos] = '\0';
    }
}

static void *setup(const MunitParameter params[], void *user_data) {
    TestFixture *fixture = munit_malloc(sizeof(TestFixture));
    memset(fixture->buffer, 0, sizeof(fixture->buffer));
    fixture->pos = 0;
    current_fixture = fixture;
    return fixture;
}

static void tear_down(void *fixture) {
    current_fixture = NULL;
    free(fixture);
}

static MunitResult test_zero(const MunitParameter params[], void *fixture) {
    printdec(0, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "0");

    return MUNIT_OK;
}

static MunitResult test_positive_numbers(const MunitParameter params[],
                                         void *fixture) {
    printdec(12345, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "12345");

    // Reset buffer
    current_fixture->pos = 0;
    current_fixture->buffer[0] = '\0';

    printdec(1, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "1");

    current_fixture->pos = 0;
    current_fixture->buffer[0] = '\0';

    printdec(9999999999LL, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "9999999999");

    return MUNIT_OK;
}

static MunitResult test_negative_numbers(const MunitParameter params[],
                                         void *fixture) {
    printdec(-12345, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "-12345");

    current_fixture->pos = 0;
    current_fixture->buffer[0] = '\0';

    printdec(-1, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "-1");

    return MUNIT_OK;
}

static MunitResult test_edge_cases(const MunitParameter params[],
                                   void *fixture) {
    printdec(INT64_MAX, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "9223372036854775807");

    current_fixture->pos = 0;
    current_fixture->buffer[0] = '\0';

    printdec(INT64_MIN, capture_char);
    munit_assert_string_equal(current_fixture->buffer, "-9223372036854775808");

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {"/zero", test_zero, setup, tear_down, MUNIT_TEST_OPTION_NONE, NULL},
        {"/positive", test_positive_numbers, setup, tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/negative", test_negative_numbers, setup, tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/edge_cases", test_edge_cases, setup, tear_down,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/printdec", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}