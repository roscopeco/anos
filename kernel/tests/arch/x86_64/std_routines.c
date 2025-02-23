#include "munit.h"
#include <stdint.h>

extern void *memmove(void *dest, const void *src, size_t count);
extern void *memclr(void *dest, size_t count);
extern void *memset(void *dest, int val, size_t count);

static MunitResult test_memmove(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char src[64] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char dest[64];

    memmove(dest, src, 64);
    munit_assert_memory_equal(64, dest, src);

    memmove(src + 5, src, 10);
    munit_assert_memory_equal(10, src + 5, "ABCDEFGHIJ");

    return MUNIT_OK;
}

static MunitResult test_memclr(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char buffer[64];
    memset(buffer, 0xFF, 64);
    memclr(buffer, 64);
    for (size_t i = 0; i < 64; i++) {
        munit_assert_uint8(buffer[i], ==, 0);
    }

    return MUNIT_OK;
}

static MunitResult test_memset(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char buffer[64];
    memclr(buffer, 64);
    memset(buffer, 0xAB, 64);
    for (size_t i = 0; i < 64; i++) {
        munit_assert_uint8(buffer[i], ==, 0xAB);
    }

    return MUNIT_OK;
}

static MunitTest tests[] = {
        {"/memmove", test_memmove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/memclr", test_memclr, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/memset", test_memset, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/memory_tests", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
