/*
 * Capability Map (Hash Table)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "munit.h"

#include "capabilities/map.h"

#define MOCK_HEAP_BLOCKS 4096
static void *mock_blocks[MOCK_HEAP_BLOCKS];
static bool block_used[MOCK_HEAP_BLOCKS];

void *fba_alloc_blocks(size_t blocks) {
    for (size_t i = 0; i + blocks <= MOCK_HEAP_BLOCKS; ++i) {
        bool available = true;
        for (size_t j = 0; j < blocks; ++j) {
            if (block_used[i + j]) {
                available = false;
                break;
            }
        }
        if (available) {
            if (!mock_blocks[i]) {
                mock_blocks[i] = calloc(4096, blocks);
            }
            for (size_t j = 0; j < blocks; ++j) {
                block_used[i + j] = true;
                mock_blocks[i + j] = mock_blocks[i];
            }
            return mock_blocks[i];
        }
    }
    return NULL;
}

void fba_free_blocks(void *ptr, size_t blocks) {
    for (size_t i = 0; i < MOCK_HEAP_BLOCKS; ++i) {
        if (mock_blocks[i] == ptr && block_used[i]) {
            for (size_t j = 0; j < blocks && i + j < MOCK_HEAP_BLOCKS; ++j) {
                block_used[i + j] = false;
                mock_blocks[i + j] = NULL;
            }
            free(ptr);
            return;
        }
    }
}

static uint8_t slab[64];
void *slab_alloc_block(void) { return slab; }

static int spinlock_calls = 0;
static int unlock_calls = 0;

void spinlock_init(SpinLock *lock) {}
void spinlock_lock(SpinLock *lock) {}
void spinlock_unlock(SpinLock *lock) {}

uint64_t spinlock_lock_irqsave(SpinLock *lock) {
    spinlock_calls++;
    return 0xDEADBEEF;
}

void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags) {
    munit_assert_uint64(flags, ==, 0xDEADBEEF);
    unlock_calls++;
}

static void reset_mock_allocator(void) {
    for (size_t i = 0; i < MOCK_HEAP_BLOCKS; ++i) {
        if (block_used[i]) {
            if (mock_blocks[i]) {
                free(mock_blocks[i]);
                mock_blocks[i] = NULL;
            }
            block_used[i] = false;
        }
    }
}

static void *cap_map_setup(const MunitParameter params[], void *user_data) {
    reset_mock_allocator();
    spinlock_calls = 0;
    unlock_calls = 0;
    CapabilityMap *map = malloc(sizeof(CapabilityMap));
    munit_assert_not_null(map);
    munit_assert_true(cap_map_init(map));
    return map;
}

static void CapabilityMapeardown(void *fixture) {
    CapabilityMap *map = fixture;
    if (map->entries)
        fba_free_blocks(map->entries, map->block_count);
    free(map);
}

static MunitResult test_basic_insert_lookup(const MunitParameter params[],
                                            void *fixture) {
    CapabilityMap *map = fixture;
    int value = 42;
    munit_assert_true(cap_map_insert(map, 1234, &value));
    munit_assert_ptr_equal(cap_map_lookup(map, 1234), &value);
    return MUNIT_OK;
}

static MunitResult test_update_existing_key(const MunitParameter params[],
                                            void *fixture) {
    CapabilityMap *map = fixture;
    int a = 1, b = 2;
    munit_assert_true(cap_map_insert(map, 77, &a));
    munit_assert_ptr_equal(cap_map_lookup(map, 77), &a);
    munit_assert_true(cap_map_insert(map, 77, &b));
    munit_assert_ptr_equal(cap_map_lookup(map, 77), &b);
    return MUNIT_OK;
}

static MunitResult test_delete_key(const MunitParameter params[],
                                   void *fixture) {
    CapabilityMap *map = fixture;
    int x = 55;
    munit_assert_true(cap_map_insert(map, 1000, &x));
    munit_assert_ptr_equal(cap_map_lookup(map, 1000), &x);
    munit_assert_true(cap_map_delete(map, 1000));
    munit_assert_null(cap_map_lookup(map, 1000));
    return MUNIT_OK;
}

static MunitResult test_rehashing_and_growth(const MunitParameter params[],
                                             void *fixture) {
    CapabilityMap *map = fixture;
    size_t count = 1000;
    for (size_t i = 0; i < count; ++i) {
        munit_assert_true(cap_map_insert(map, i, (void *)(uintptr_t)(i + 1)));
    }
    for (size_t i = 0; i < count; ++i) {
        munit_assert_ptr_equal(cap_map_lookup(map, i),
                               (void *)(uintptr_t)(i + 1));
    }
    return MUNIT_OK;
}

static MunitResult
test_sparse_and_collision_behavior(const MunitParameter params[],
                                   void *fixture) {
    CapabilityMap *map = fixture;
    int val1 = 111, val2 = 222, val3 = 333;
    munit_assert_true(cap_map_insert(map, 0xdeadbeef, &val1));
    munit_assert_true(cap_map_insert(map, 0xdeadbeef ^ 0x1000, &val2));
    munit_assert_true(cap_map_insert(map, 0xdeadbeef ^ 0x2000, &val3));
    munit_assert_ptr_equal(cap_map_lookup(map, 0xdeadbeef), &val1);
    munit_assert_ptr_equal(cap_map_lookup(map, 0xdeadbeef ^ 0x1000), &val2);
    munit_assert_ptr_equal(cap_map_lookup(map, 0xdeadbeef ^ 0x2000), &val3);
    return MUNIT_OK;
}

static MunitResult test_stress_insert_delete(const MunitParameter params[],
                                             void *fixture) {
    CapabilityMap *map = fixture;
    const size_t count = 100000;
    for (uint64_t i = 0; i < count; ++i) {
        munit_assert_true(cap_map_insert(map, i, (void *)(uintptr_t)i));
    }
    for (uint64_t i = 0; i < count; i += 2) {
        munit_assert_true(cap_map_delete(map, i));
    }
    for (uint64_t i = 0; i < count; ++i) {
        if (i % 2 == 0)
            munit_assert_null(cap_map_lookup(map, i));
        else
            munit_assert_ptr_equal(cap_map_lookup(map, i),
                                   (void *)(uintptr_t)i);
    }
    munit_assert_true(cap_map_cleanup(map));

    return MUNIT_OK;
}

static MunitResult test_locking_was_done(const MunitParameter params[],
                                         void *fixture) {
    CapabilityMap *map = fixture;
    int x = 123;
    munit_assert_true(cap_map_insert(map, 1, &x));
    munit_assert_ptr_equal(cap_map_lookup(map, 1), &x);
    munit_assert_true(cap_map_delete(map, 1));
    munit_assert_true(cap_map_cleanup(map));

    munit_assert(spinlock_calls > 0);
    munit_assert(spinlock_calls == unlock_calls);
    return MUNIT_OK;
}

static MunitTest CapabilityMapests[] = {
        {"/insert_lookup", test_basic_insert_lookup, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/update_existing", test_update_existing_key, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/delete", test_delete_key, cap_map_setup, CapabilityMapeardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/rehashing", test_rehashing_and_growth, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/collisions", test_sparse_and_collision_behavior, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/stress", test_stress_insert_delete, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/lock_check", test_locking_was_done, cap_map_setup,
         CapabilityMapeardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite cap_map_suite = {"/cap_map", CapabilityMapests, NULL, 1,
                                         MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&cap_map_suite, NULL, argc, argv);
}
