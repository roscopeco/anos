/*
 * stage3 - Tests for string hash functions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <string.h>

#include "munit.h"
#include "structs/strhash.h"

/* --- djb2 tests --- */

/* When given an empty string, djb2 should immediately return its initial value. */
static MunitResult test_str_hash_djb2_empty(const MunitParameter params[],
                                            void *user_data) {
    unsigned char *str = (unsigned char *)"";
    uint64_t hash = str_hash_djb2(str, 10);
    munit_assert_uint64(hash, ==, 5381ULL);
    return MUNIT_OK;
}

/* For "hello" with full max_len the computed djb2 hash should match the known value.
   Computation:
     - Start: hash = 5381
     - 'h': 5381*33 + 104 = 177677
     - 'e': 177677*33 + 101 = 5863442
     - 'l': 5863442*33 + 108 = 193493694
     - 'l': 193493694*33 + 108 = 6385292010
     - 'o': 6385292010*33 + 111 = 210714636441
*/
static MunitResult test_str_hash_djb2_normal(const MunitParameter params[],
                                             void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash = str_hash_djb2(str, 10);
    munit_assert_uint64(hash, ==, 210714636441ULL);
    return MUNIT_OK;
}

/* When max_len is less than the string length only the first max_len characters are processed.
   For "hello" with max_len == 3, only "hel" is hashed:
     - 'h': 5381*33 + 104 = 177677
     - 'e': 177677*33 + 101 = 5863442
     - 'l': 5863442*33 + 108 = 193493694
*/
static MunitResult test_str_hash_djb2_max_len(const MunitParameter params[],
                                              void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash = str_hash_djb2(str, 3);
    munit_assert_uint64(hash, ==, 193493694ULL);
    return MUNIT_OK;
}

/* With max_len==0 no characters are processed so the initial hash is returned. */
static MunitResult
test_str_hash_djb2_zero_max_len(const MunitParameter params[],
                                void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash = str_hash_djb2(str, 0);
    munit_assert_uint64(hash, ==, 5381ULL);
    return MUNIT_OK;
}

/* Null is the same as an empty string */
static MunitResult test_str_hash_djb2_null(const MunitParameter params[],
                                           void *user_data) {
    uint64_t hash = str_hash_djb2(NULL, 0);
    munit_assert_uint64(hash, ==, 5381ULL);
    return MUNIT_OK;
}

/* --- sdbm tests --- */

/* For an empty string, sdbm returns 0. */
static MunitResult test_str_hash_sdbm_empty(const MunitParameter params[],
                                            void *user_data) {
    unsigned char *str = (unsigned char *)"";
    uint64_t hash = str_hash_sdbm(str, 10);
    munit_assert_uint64(hash, ==, 0ULL);
    return MUNIT_OK;
}

/* With max_len==0, sdbm returns 0 because no characters are processed. */
static MunitResult
test_str_hash_sdbm_zero_max_len(const MunitParameter params[],
                                void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash = str_hash_sdbm(str, 0);
    munit_assert_uint64(hash, ==, 0ULL);
    return MUNIT_OK;
}

/* For sdbm, we test a partial hash. For "hel" the computation is:
     h = 0;
     'h': h = 0 * 65599 + 104 = 104;
     'e': h = 104 * 65599 + 101 = 6822397;
     'l': h = 6822397 * 65599 + 108 = 447542420911;
   (Since 447542420911 is far below 2^64, no overflow occurs here.)
*/
static MunitResult test_str_hash_sdbm_partial(const MunitParameter params[],
                                              void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash = str_hash_sdbm(str, 3);
    munit_assert_uint64(hash, ==, 447542420911ULL);
    return MUNIT_OK;
}

/* For a full string, we check that using a max_len longer than the string length does not change the result.
   (Since the function stops when it reaches the terminating NUL, sdbm("hello", 10) should equal
    sdbm("hello", strlen("hello")).)
*/
static MunitResult
test_str_hash_sdbm_full_vs_strlen(const MunitParameter params[],
                                  void *user_data) {
    unsigned char *str = (unsigned char *)"hello";
    uint64_t hash_full = str_hash_sdbm(str, 10);
    uint64_t hash_exact = str_hash_sdbm(str, strlen((char *)str));
    munit_assert_uint64(hash_full, ==, hash_exact);
    return MUNIT_OK;
}

/* Null is the same as an empty string */
static MunitResult test_str_hash_sdbm_null(const MunitParameter params[],
                                           void *user_data) {
    uint64_t hash = str_hash_sdbm(NULL, 0);
    munit_assert_uint64(hash, ==, 0ULL);
    return MUNIT_OK;
}

/* --- Test suite registration --- */
static MunitTest test_suite_tests[] = {
        {"/djb2/empty", test_str_hash_djb2_empty, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/djb2/normal", test_str_hash_djb2_normal, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/djb2/max_len", test_str_hash_djb2_max_len, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/djb2/zero_max", test_str_hash_djb2_zero_max_len, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/djb2/null", test_str_hash_djb2_null, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/sdbm/empty", test_str_hash_sdbm_empty, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/sdbm/zero_max", test_str_hash_sdbm_zero_max_len, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/sdbm/partial", test_str_hash_sdbm_partial, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/sdbm/full", test_str_hash_sdbm_full_vs_strlen, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/sdbm/null", test_str_hash_sdbm_null, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/structs/strhash", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
