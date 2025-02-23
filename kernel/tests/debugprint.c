/*
 * Tests for the debug terminal
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdlib.h>

#define UNIT_TEST_DEBUGPRINT
#include "debugprint.h"
#include "munit.h"

static char *video_buffer;

#define IGNORED ((0))

static MunitResult test_init_empty(const MunitParameter params[], void *param) {
    debugterm_init(video_buffer, IGNORED, IGNORED);

    return MUNIT_OK;
}

static MunitResult test_debug_str(const MunitParameter params[], void *param) {
    debugterm_init(video_buffer, IGNORED, IGNORED);

    debugstr("Hello, World");

    char expect[] = {'H', 0x07, 'e', 0x07, 'l', 0x07, 'l', 0x07,
                     'o', 0x07, ',', 0x07, ' ', 0x07, 'W', 0x07,
                     'o', 0x07, 'r', 0x07, 'l', 0x07, 'd', 0x07};

    munit_assert_memory_equal(24, expect, video_buffer);

    return MUNIT_OK;
}

static MunitResult test_debug_str_newline(const MunitParameter params[],
                                          void *param) {
    debugterm_init(video_buffer, IGNORED, IGNORED);

    debugstr("Hello, World\nNew!");

    char expect1[] = {'H', 0x07, 'e', 0x07, 'l', 0x07, 'l', 0x07, 'o', 0x07,
                      ',', 0x07, ' ', 0x07, 'W', 0x07, 'o', 0x07, 'r', 0x07,
                      'l', 0x07, 'd', 0x07, 0x0, 0x00, 0x0, 0x00};

    char expect2[] = {'N', 0x07, 'e', 0x07, 'w', 0x07, '!', 0x07, 0x0, 0x00};

    munit_assert_memory_equal(28, expect1, video_buffer);
    munit_assert_memory_equal(10, expect2, video_buffer + 160);

    return MUNIT_OK;
}

static MunitResult test_debug_attr(const MunitParameter params[], void *param) {
    debugterm_init(video_buffer, IGNORED, IGNORED);

    debugattr(0x1C);
    debugstr("Hello, World\n");
    debugattr(0x3F);
    debugstr("New!");

    char expect1[] = {'H', 0x1C, 'e', 0x1C, 'l', 0x1C, 'l', 0x1C, 'o', 0x1C,
                      ',', 0x1C, ' ', 0x1C, 'W', 0x1C, 'o', 0x1C, 'r', 0x1C,
                      'l', 0x1C, 'd', 0x1C, 0x0, 0x00, 0x0, 0x00};

    char expect2[] = {'N', 0x3F, 'e', 0x3F, 'w', 0x3F, '!', 0x3F, 0x0, 0x00};

    munit_assert_memory_equal(28, expect1, video_buffer);
    munit_assert_memory_equal(10, expect2, video_buffer + 160);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    video_buffer = malloc(0x4000);
    memset(video_buffer, 0, 0x4000);
    return NULL;
}

static void teardown(void *param) { free(video_buffer); }

static MunitTest test_suite_tests[] = {
        {(char *)"/debugprint/init_empty", test_init_empty, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/debugprint/debug_str", test_debug_str, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/debugprint/debug_str_newline", test_debug_str_newline, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/debugprint/debug_attr", test_debug_attr, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}