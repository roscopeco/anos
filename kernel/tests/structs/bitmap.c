/*
 * Tests for the bitmap
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "structs/bitmap.h"
#include "munit.h"

#define BITMAP_SIZE 4
static uint64_t test_bitmap[BITMAP_SIZE];

static void *setup(const MunitParameter params[], void *user_data) {
    memset(test_bitmap, 0, BITMAP_SIZE << 3);
    return NULL;
}

static MunitResult test_set_bit_zero(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_set(test_bitmap, 0);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_set_bit_one(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_set(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000002, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_set_bit_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000001;

    bitmap_set(test_bitmap, 0);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_set_bit_one_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000001;

    bitmap_set(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000003, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_set_two_bits(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_set(test_bitmap, 0);
    bitmap_set(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000003, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_set_one_bit_second_word(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_set(test_bitmap, 64);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[0]);
    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[1]);

    return MUNIT_OK;
}

static MunitResult test_set_bit_boundary(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);
    munit_assert_uint64(0, ==, test_bitmap[1]);
    munit_assert_uint64(0, ==, test_bitmap[2]);
    munit_assert_uint64(0, ==, test_bitmap[3]);

    bitmap_set(test_bitmap, 63);
    bitmap_set(test_bitmap, 64);

    bitmap_set(test_bitmap, 127);
    bitmap_set(test_bitmap, 128);

    bitmap_set(test_bitmap, 191);
    bitmap_set(test_bitmap, 192);

    munit_assert_uint64(0x8000000000000000, ==, test_bitmap[0]);
    munit_assert_uint64(0x8000000000000001, ==, test_bitmap[1]);
    munit_assert_uint64(0x8000000000000001, ==, test_bitmap[2]);
    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[3]);

    return MUNIT_OK;
}

static MunitResult test_clear_bit_zero(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000001;

    bitmap_clear(test_bitmap, 0);

    munit_assert_uint64(0, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_clear_bit_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x000000000000000e;

    bitmap_clear(test_bitmap, 0);

    munit_assert_uint64(0x000000000000000e, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_clear_bit_one(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000003;

    bitmap_clear(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_clear_bit_one_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x000000000000003;

    bitmap_clear(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_clear_two_bits(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000003;

    bitmap_clear(test_bitmap, 0);
    bitmap_clear(test_bitmap, 1);

    munit_assert_uint64(0, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_clear_one_bit_second_word(const MunitParameter params[], void *param) {
    test_bitmap[1] = 0x0000000000000001;

    bitmap_clear(test_bitmap, 64);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[1]);

    return MUNIT_OK;
}

static MunitResult test_clear_bit_boundary(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x8000000000000000;
    test_bitmap[1] = 0x8000000000000001;
    test_bitmap[2] = 0x8000000000000001;
    test_bitmap[3] = 0x0000000000000001;

    bitmap_clear(test_bitmap, 63);
    bitmap_clear(test_bitmap, 64);

    bitmap_clear(test_bitmap, 127);
    bitmap_clear(test_bitmap, 128);

    bitmap_clear(test_bitmap, 191);
    bitmap_clear(test_bitmap, 192);

    munit_assert_uint64(0, ==, test_bitmap[0]);
    munit_assert_uint64(0, ==, test_bitmap[1]);
    munit_assert_uint64(0, ==, test_bitmap[2]);
    munit_assert_uint64(0, ==, test_bitmap[3]);

    return MUNIT_OK;
}

static MunitResult test_flip_bit_zero(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_flip(test_bitmap, 0);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    bitmap_flip(test_bitmap, 0);

    munit_assert_uint64(0, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_flip_bit_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000001;

    bitmap_flip(test_bitmap, 0);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_flip_bit_one(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000002;

    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[0]);

    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000002, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_flip_bit_one_zero_already(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x000000000000003;

    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[0]);

    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000003, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_flip_two_bits(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000003;

    bitmap_flip(test_bitmap, 0);
    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0, ==, test_bitmap[0]);

    bitmap_flip(test_bitmap, 0);
    bitmap_flip(test_bitmap, 1);

    munit_assert_uint64(0x0000000000000003, ==, test_bitmap[0]);

    return MUNIT_OK;
}

static MunitResult test_flip_one_bit_second_word(const MunitParameter params[], void *param) {
    test_bitmap[1] = 0x0000000000000001;

    bitmap_flip(test_bitmap, 64);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[0]);
    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[1]);
    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[2]);
    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[3]);

    bitmap_flip(test_bitmap, 64);

    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[0]);
    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[1]);
    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[2]);
    munit_assert_uint64(0x0000000000000000, ==, test_bitmap[3]);

    return MUNIT_OK;
}

static MunitResult test_flip_bit_boundary(const MunitParameter params[], void *param) {
    munit_assert_uint64(0, ==, test_bitmap[0]);
    munit_assert_uint64(0, ==, test_bitmap[1]);
    munit_assert_uint64(0, ==, test_bitmap[2]);
    munit_assert_uint64(0, ==, test_bitmap[3]);

    bitmap_flip(test_bitmap, 63);
    bitmap_flip(test_bitmap, 64);

    bitmap_flip(test_bitmap, 127);
    bitmap_flip(test_bitmap, 128);

    bitmap_flip(test_bitmap, 191);
    bitmap_flip(test_bitmap, 192);

    munit_assert_uint64(0x8000000000000000, ==, test_bitmap[0]);
    munit_assert_uint64(0x8000000000000001, ==, test_bitmap[1]);
    munit_assert_uint64(0x8000000000000001, ==, test_bitmap[2]);
    munit_assert_uint64(0x0000000000000001, ==, test_bitmap[3]);

    bitmap_flip(test_bitmap, 63);
    bitmap_flip(test_bitmap, 64);

    bitmap_flip(test_bitmap, 127);
    bitmap_flip(test_bitmap, 128);

    bitmap_flip(test_bitmap, 191);
    bitmap_flip(test_bitmap, 192);

    munit_assert_uint64(0, ==, test_bitmap[0]);
    munit_assert_uint64(0, ==, test_bitmap[1]);
    munit_assert_uint64(0, ==, test_bitmap[2]);
    munit_assert_uint64(0, ==, test_bitmap[3]);

    return MUNIT_OK;
}

static MunitResult test_check_bit(const MunitParameter params[], void *param) {
    test_bitmap[0] = 0x0000000000000003;
    test_bitmap[1] = 0x8000000000000000;

    munit_assert_true(bitmap_check(test_bitmap, 0));
    munit_assert_true(bitmap_check(test_bitmap, 1));

    for (int i = 2; i < 127; i++) {
        munit_assert_false(bitmap_check(test_bitmap, i));
    }

    munit_assert_true(bitmap_check(test_bitmap, 127));

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/set_bit_zero", test_set_bit_zero, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_bit_zero_already", test_set_bit_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_bit_one", test_set_bit_one, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_bit_one_already", test_set_bit_one_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_two_bits", test_set_two_bits, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_1b_2nd_word", test_set_one_bit_second_word, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/set_bit_boundary", test_set_bit_boundary, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/clr_bit_zero", test_clear_bit_zero, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_bit_zero_already", test_clear_bit_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_bit_one", test_clear_bit_one, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_bit_one_already", test_clear_bit_one_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_two_bits", test_clear_two_bits, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_1b_2nd_word", test_clear_one_bit_second_word, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/clr_bit_boundary", test_clear_bit_boundary, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/flp_bit_zero", test_flip_bit_zero, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_bit_zero_already", test_flip_bit_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_bit_one", test_flip_bit_one, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_bit_one_already", test_flip_bit_one_zero_already, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_two_bits", test_flip_two_bits, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_1b_2nd_word", test_flip_one_bit_second_word, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/flp_bit_boundary", test_flip_bit_boundary, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/check_bit", test_check_bit, setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/structs/bitmap", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}