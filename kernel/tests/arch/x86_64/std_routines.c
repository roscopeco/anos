#include <stdint.h>
#include <string.h>

#include "munit.h"
#include "std/string.h"

void *anos_std_memcpy(void *restrict dest, const void *restrict src,
                      size_t count);
void *anos_std_memmove(void *dest, const void *src, size_t count);
void *anos_std_memset(void *dest, int val, size_t count);
void *memclr(void *dest, size_t count);

static MunitResult test_memcpy(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char src[64] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789AB";
    char dest[64];

    // Basic memcpy test
    anos_std_memcpy(dest, src, 64);
    munit_assert_memory_equal(64, dest, src);

    // Partial copy
    anos_std_memcpy(dest, src + 10, 10);
    munit_assert_memory_equal(10, dest, "KLMNOPQRST");

    // Self-copy (should not break, but is undefined behavior)
    anos_std_memcpy(src, src, 64);
    munit_assert_memory_equal(
            64, src,
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789AB");

    return MUNIT_OK;
}

static MunitResult test_memmove(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char src[64] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char dest[64];

    // Basic memmove test
    anos_std_memmove(dest, src, 64);
    munit_assert_memory_equal(64, dest, src);

    // Overlapping backward copy
    anos_std_memmove(src + 5, src, 10);
    munit_assert_memory_equal(10, src, "ABCDEABCDE");

    // Overlapping forward copy
    anos_std_memmove(src, src + 5, 10);
    munit_assert_memory_equal(10, src, "ABCDEFGHIJ");

    return MUNIT_OK;
}

static MunitResult test_memclr(const MunitParameter params[], void *data) {
    (void)params;
    (void)data;

    char buffer[64];
    anos_std_memset(buffer, 0xFF, 64);
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
    anos_std_memset(buffer, 0xAB, 64);
    for (size_t i = 0; i < 64; i++) {
        munit_assert_uint8(buffer[i], ==, 0xAB);
    }

    return MUNIT_OK;
}

static MunitTest tests[] = {
        {"/mem/cpy", test_memcpy, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/move", test_memmove, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/clr", test_memclr, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/mem/set", test_memset, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite suite = {"/std_routines", tests, NULL, 1,
                                 MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
