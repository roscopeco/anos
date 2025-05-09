/*
 * Anos path helper routines tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "munit.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Forward declarations (your actual implementation should be linked)
bool parse_file_path(char *input, size_t input_len, char **mount_point,
                     char **path);

// === Valid Cases ===

static MunitResult test_valid_simple(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    char input[] = "boot:/test_server.elf";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_true(result);
    munit_assert_string_equal(mount, "boot");
    munit_assert_string_equal(path, "/test_server.elf");

    return MUNIT_OK;
}

static MunitResult test_valid_colon_in_path(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    char input[] = "home:/dir/with:colon/file.txt";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_true(result);
    munit_assert_string_equal(mount, "home");
    munit_assert_string_equal(path, "/dir/with:colon/file.txt");

    return MUNIT_OK;
}

static MunitResult test_valid_long_mount(const MunitParameter params[],
                                         void *data) {
    (void)params;
    (void)data;

    char input[] = "filesystem_mount_name:/foo";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_true(result);
    munit_assert_string_equal(mount, "filesystem_mount_name");
    munit_assert_string_equal(path, "/foo");

    return MUNIT_OK;
}

// === Invalid Cases ===

static MunitResult test_no_colon(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char input[] = "nofs/file.txt";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_colon_at_start(const MunitParameter params[],
                                       void *data) {
    (void)params;
    (void)data;

    char input[] = ":/bad";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_colon_at_end(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    char input[] = "boot:";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_unterminated_string(const MunitParameter params[],
                                            void *data) {
    (void)params;
    (void)data;

    // Not NUL-terminated and no terminator within range
    char input[8] = {'b', 'o', 'o', 't', ':', '/', 'f', 'i'}; // no '\0'
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_empty_string(const MunitParameter params[],
                                     void *data) {
    (void)params;
    (void)data;

    char input[] = "";
    char *mount = NULL, *path = NULL;

    bool result = parse_file_path(input, sizeof(input), &mount, &path);
    munit_assert_false(result);

    return MUNIT_OK;
}

// === Test Suite ===

static MunitTest tests[] = {
        {"/valid/simple", test_valid_simple, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/valid/colon-in-path", test_valid_colon_in_path, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/valid/long-mount", test_valid_long_mount, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/invalid/no-colon", test_no_colon, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/invalid/colon-start", test_colon_at_start, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/invalid/colon-end", test_colon_at_end, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/invalid/unterminated", test_unterminated_string, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/invalid/empty", test_empty_string, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/parse_file_path", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
