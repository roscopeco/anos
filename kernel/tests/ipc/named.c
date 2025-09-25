/*
 * stage3 - Tests for named IPC channels
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ipc/channel.h"
#include "ipc/named.h"
#include "munit.h"
#include "spinlock.h"
#include "structs/hash.h"
#include "structs/strhash.h"

// Stub implementation of ipc_channel_exists for testing.
// For our purposes, we consider cookie 0 to be invalid.
bool ipc_channel_exists(uint64_t cookie) { return (cookie != 0); }

void spinlock_init(SpinLock *lock) { (void)lock; }
void spinlock_lock(SpinLock *lock) { (void)lock; }
void spinlock_unlock(SpinLock *lock) { (void)lock; }

/* --- Slab Allocator Mocks --- */
void *slab_alloc_block(void) {
    /* For testing we simply allocate 64 bytes */
    return malloc(64);
}
void slab_free(void *ptr) { free(ptr); }

/* Test: Register a valid channel and verify lookup */
static MunitResult test_register_valid(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    uint64_t cookie = 12345;
    char *name = "channel1";

    munit_assert_true(named_channel_register(cookie, name));
    munit_assert_uint64(named_channel_find(name), ==, cookie);

    return MUNIT_OK;
}

/* Test: Registration fails for an invalid (non-existent) channel */
static MunitResult test_register_invalid_channel(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    uint64_t cookie = 0; // Invalid cookie
    char *name = "invalid_channel";

    munit_assert_false(named_channel_register(cookie, name));

    return MUNIT_OK;
}

/* Test: Duplicate registration (same name) is not allowed */
static MunitResult test_duplicate_register(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    uint64_t cookie1 = 11111;
    uint64_t cookie2 = 22222;
    char *name = "channel_dup";

    munit_assert_true(named_channel_register(cookie1, name));
    /* Second registration with the same name should fail because it would create a hash collision */
    munit_assert_false(named_channel_register(cookie2, name));
    /* Lookup should return the original cookie */
    munit_assert_uint64(named_channel_find(name), ==, cookie1);

    return MUNIT_OK;
}

/* Test: Deregister a channel, and then ensure lookup returns 0 (not found) */
static MunitResult test_deregister(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    uint64_t cookie = 33333;
    char *name = "channel_to_remove";

    munit_assert_true(named_channel_register(cookie, name));
    munit_assert_uint64(named_channel_find(name), ==, cookie);

    uint64_t removed = named_channel_deregister(name);
    munit_assert_uint64(removed, ==, cookie);
    /* After removal, lookup should return 0 */
    munit_assert_uint64(named_channel_find(name), ==, 0);

    return MUNIT_OK;
}

/* Test: Lookup for a name that was never registered returns 0 */
static MunitResult test_lookup_missing(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    munit_assert_uint64(named_channel_find("nonexistent"), ==, 0);

    return MUNIT_OK;
}

/* Test: Using a long name (exceeding 255 characters)
   Even if the name is longer than 255 characters, the hash function
   only processes the first 255. The register and lookup must behave consistently.
*/
static MunitResult test_long_name_truncation(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;

    named_channel_init();

    char long_name[310];
    for (int i = 0; i < 300; i++) {
        long_name[i] = 'a' + (i % 26);
    }
    long_name[300] = '\0';

    uint64_t cookie = 44444;
    munit_assert_true(named_channel_register(cookie, long_name));
    munit_assert_uint64(named_channel_find(long_name), ==, cookie);

    return MUNIT_OK;
}

/* Test suite registration */
static MunitTest test_suite_tests[] = {
        {"/register_valid", test_register_valid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/register_invalid", test_register_invalid_channel, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/duplicate_register", test_duplicate_register, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/deregister", test_deregister, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/lookup_missing", test_lookup_missing, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/long_name_truncation", test_long_name_truncation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/ipc/named", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) { return munit_suite_main(&test_suite, NULL, argc, argv); }
