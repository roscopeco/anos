/*
 * stage3 - Tests for reference-counting map
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "munit.h"

#include "spinlock.h"
#include "structs/ref_count_map.h"

#include "mock_fba.h"
#include "mock_machine.h"
#include "mock_slab.h"
#include "mock_spinlock.h"

void refcount_map_cleanup(void);

// Test initialization
static MunitResult test_init(const MunitParameter params[], void *data) {
    // Test successful initialization
    bool result = refcount_map_init();

    munit_assert_true(result);

    munit_assert_size(mock_fba_get_alloc_count(), >, 0);
    munit_assert_size(mock_slab_get_alloc_count(), >, 0);

    munit_assert_false(mock_spinlock_is_locked());

    // Test double initialization
    uint64_t prev_fba = mock_fba_get_alloc_count();
    uint64_t prev_slab = mock_slab_get_alloc_count();
    result = refcount_map_init();

    munit_assert_true(result);

    munit_assert_size(mock_fba_get_alloc_count(), ==, prev_fba);
    munit_assert_size(mock_slab_get_alloc_count(), ==, prev_slab);

    refcount_map_cleanup();
    return MUNIT_OK;
}

static MunitResult test_init_fba_fail(const MunitParameter params[],
                                      void *data) {
    // Test FBA allocation failure
    mock_fba_set_should_fail(true);
    bool result = refcount_map_init();

    munit_assert_false(result);

    munit_assert_false(mock_spinlock_is_locked());

    refcount_map_cleanup();
    return MUNIT_OK;
}

static MunitResult test_init_slab_fail(const MunitParameter params[],
                                       void *data) {
    // Test slab allocation failure
    mock_slab_set_should_fail(true);
    bool result = refcount_map_init();

    munit_assert_false(result);

    munit_assert_false(mock_spinlock_is_locked());

    refcount_map_cleanup();
    return MUNIT_OK;
}

// Test basic reference counting
static MunitResult test_basic_refcount(const MunitParameter params[],
                                       void *data) {
    refcount_map_init();

    uintptr_t addr = 0x3000;

    // First reference
    uint32_t count = refcount_map_increment(addr);
    munit_assert_uint32(count, ==, 1);
    munit_assert_false(mock_spinlock_is_locked());

    // Second reference
    count = refcount_map_increment(addr);
    munit_assert_uint32(count, ==, 2);

    // Remove one reference
    count = refcount_map_decrement(addr);
    munit_assert_uint32(count, ==, 2);

    // Remove last reference
    count = refcount_map_decrement(addr);
    munit_assert_uint32(count, ==, 1);

    // Try to remove non-existent reference
    count = refcount_map_decrement(addr);
    munit_assert_uint32(count, ==, 0);

    refcount_map_cleanup();
    return MUNIT_OK;
}

// Test multiple addresses
static MunitResult test_multiple_addresses(const MunitParameter params[],
                                           void *data) {
    refcount_map_init();

    uintptr_t addr1 = 0x3000;
    uintptr_t addr2 = 0x4000;
    uintptr_t addr3 = 0x5000;

    // Add references to all addresses
    munit_assert_uint32(refcount_map_increment(addr1), ==, 1);
    munit_assert_uint32(refcount_map_increment(addr2), ==, 1);
    munit_assert_uint32(refcount_map_increment(addr3), ==, 1);

    // Add more references to addr2
    munit_assert_uint32(refcount_map_increment(addr2), ==, 2);
    munit_assert_uint32(refcount_map_increment(addr2), ==, 3);

    // Remove references in mixed order
    munit_assert_uint32(refcount_map_decrement(addr1), ==, 1);
    munit_assert_uint32(refcount_map_decrement(addr2), ==, 3);
    munit_assert_uint32(refcount_map_decrement(addr3), ==, 1);
    munit_assert_uint32(refcount_map_decrement(addr2), ==, 2);
    munit_assert_uint32(refcount_map_decrement(addr2), ==, 1);

    refcount_map_cleanup();
    return MUNIT_OK;
}

// Test allocation failures during operation
static MunitResult test_allocation_failures(const MunitParameter params[],
                                            void *data) {
    refcount_map_init();

    uintptr_t addr = 0x3000;

    // Test slab allocation failure during increment
    mock_slab_set_should_fail(true);
    uint32_t count = refcount_map_increment(addr);
    munit_assert_uint32(count, ==, 0);
    munit_assert_false(mock_spinlock_is_locked());

    mock_slab_set_should_fail(false);

    // Add some references successfully
    munit_assert_uint32(refcount_map_increment(addr), ==, 1);
    munit_assert_uint32(refcount_map_increment(addr), ==, 2);

    refcount_map_cleanup();
    return MUNIT_OK;
}

// Test hash table resizing
static MunitResult test_resize(const MunitParameter params[], void *data) {
    refcount_map_init();

    // Add enough entries to trigger a resize
    for (uintptr_t addr = 0x3000; addr < 0x4000; addr += 0x10) {
        uint32_t count = refcount_map_increment(addr);
        munit_assert_uint32(count, ==, 1);
    }

    // Verify we can still access all entries
    for (uintptr_t addr = 0x3000; addr < 0x4000; addr += 0x10) {
        uint32_t count = refcount_map_increment(addr);
        munit_assert_uint32(count, ==, 2);
        count = refcount_map_decrement(addr);
        munit_assert_uint32(count, ==, 2);
    }

    refcount_map_cleanup();
    return MUNIT_OK;
}

// Test memory leaks
static MunitResult test_memory_cleanup(const MunitParameter params[],
                                       void *data) {
    refcount_map_init();

    uint64_t initial_fba = mock_fba_get_alloc_count();
    uint64_t initial_slab = mock_slab_get_alloc_count();

    // Perform various operations
    uintptr_t addr = 0x3000;
    refcount_map_increment(addr);
    refcount_map_increment(addr);
    refcount_map_decrement(addr);

    // Clean up and verify all memory is freed
    refcount_map_cleanup();

    munit_assert_size(mock_fba_get_free_count(), ==,
                      mock_fba_get_alloc_count());
    munit_assert_size(mock_slab_get_free_count(), ==,
                      mock_slab_get_alloc_count());

    return MUNIT_OK;
}

// Test null/edge cases
static MunitResult test_edge_cases(const MunitParameter params[], void *data) {
    // Test operations without initialization
    uint32_t count = refcount_map_increment(0x3000);
    munit_assert_uint32(count, ==, 0);
    munit_assert_false(mock_spinlock_is_locked());

    count = refcount_map_decrement(0x3000);
    munit_assert_uint32(count, ==, 0);
    munit_assert_false(mock_spinlock_is_locked());

    // Test cleanup without initialization
    refcount_map_cleanup();
    munit_assert_false(mock_spinlock_is_locked());

    // Initialize and test edge addresses
    refcount_map_init();

    count = refcount_map_increment(0);
    munit_assert_uint32(count, ==, 1);

    count = refcount_map_increment(UINTPTR_MAX);
    munit_assert_uint32(count, ==, 1);

    refcount_map_cleanup();
    return MUNIT_OK;
}

static void reset_mocks(void) {
    mock_slab_reset();
    mock_fba_reset();
    mock_spinlock_reset();
}

static void *test_setup(const MunitParameter params[], void *user_data) {
    reset_mocks();
    return NULL;
}

static MunitTest refcount_map_tests[] = {
        {"/init_okay", test_init, test_setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/init_fba_fail", test_init_fba_fail, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/init_slab_fail", test_init_slab_fail, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/basic_refcount", test_basic_refcount, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/multiple_addresses", test_multiple_addresses, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/allocation_failures", test_allocation_failures, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/resize", test_resize, test_setup, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/memory_cleanup", test_memory_cleanup, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/edge_cases", test_edge_cases, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, test_setup, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/refcount_map", refcount_map_tests, NULL,
                                      1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}