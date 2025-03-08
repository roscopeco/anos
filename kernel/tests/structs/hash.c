/*
 * stage3 - Tests for general hash
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "munit.h"
#include "structs/hash.h"

#define THREAD_COUNT ((4))
#define ENTRIES_PER_THREAD ((1000))

void *fba_alloc_blocks(uint64_t count) { return calloc(count, VM_PAGE_SIZE); }

void *fba_alloc_block() { return calloc(1, VM_PAGE_SIZE); }

void *slab_alloc_block() { return calloc(1, 64); }

void fba_free(void *block) { free(block); }

void slab_free(void *block) { free(block); }

// Test: Create and destroy hash table
static MunitResult test_create_destroy(const MunitParameter params[],
                                       void *user_data) {
    HashTable *ht = hash_table_create(1);
    munit_assert_not_null(ht);
    munit_assert_not_null(ht->entries);
    munit_assert_size(ht->capacity, >=, ENTRIES_PER_PAGE);
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Test: Lookup for non-existent key
static MunitResult test_lookup_non_existent(const MunitParameter params[],
                                            void *user_data) {
    HashTable *ht = hash_table_create(1);
    munit_assert_null(hash_table_lookup(ht, 99999));
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Test: Insert, remove, then re-insert the same key
static MunitResult test_insert_after_delete(const MunitParameter params[],
                                            void *user_data) {
    HashTable *ht = hash_table_create(1);
    uint64_t cookie = 11111;
    void *channel = (void *)0xabcdef;
    munit_assert_true(hash_table_insert(ht, cookie, channel));
    munit_assert_true(hash_table_remove(ht, cookie));
    munit_assert_null(hash_table_lookup(ht, cookie));
    munit_assert_true(hash_table_insert(ht, cookie, channel));
    munit_assert_ptr_equal(hash_table_lookup(ht, cookie), channel);
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Test: Ensure tombstone slots are reused
static MunitResult test_tombstone_reuse(const MunitParameter params[],
                                        void *user_data) {
    HashTable *ht = hash_table_create(1);
    uint64_t first = 22222, second = 33333;
    void *channel1 = (void *)0x1234, *channel2 = (void *)0x5678;
    munit_assert_true(hash_table_insert(ht, first, channel1));
    munit_assert_true(hash_table_remove(ht, first));
    munit_assert_true(hash_table_insert(ht, second, channel2));
    munit_assert_ptr_equal(hash_table_lookup(ht, second), channel2);
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Test: Insert at full capacity
static MunitResult test_insert_full_capacity(const MunitParameter params[],
                                             void *user_data) {
    HashTable *ht = hash_table_create(1);
    for (uint64_t i = 1; i <= ENTRIES_PER_PAGE; i++) {
        munit_assert_true(hash_table_insert(ht, i, (void *)(uintptr_t)i));
    }
    for (uint64_t i = 1; i <= ENTRIES_PER_PAGE; i++) {
        munit_assert_ptr_equal(hash_table_lookup(ht, i), (void *)(uintptr_t)i);
    }
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Test: Resizing with deletions
static MunitResult test_resize_with_deletions(const MunitParameter params[],
                                              void *user_data) {
    HashTable *ht = hash_table_create(1);
    for (uint64_t i = 1; i <= ENTRIES_PER_PAGE; i++) {
        munit_assert_true(hash_table_insert(ht, i, (void *)(uintptr_t)i));
    }
    for (uint64_t i = 1; i <= ENTRIES_PER_PAGE / 2; i++) {
        munit_assert_true(hash_table_remove(ht, i));
    }
    munit_assert_true(
            hash_table_insert(ht, ENTRIES_PER_PAGE + 1,
                              (void *)(uintptr_t)(ENTRIES_PER_PAGE + 1)));
    for (uint64_t i = ENTRIES_PER_PAGE / 2 + 1; i <= ENTRIES_PER_PAGE + 1;
         i++) {
        munit_assert_ptr_equal(hash_table_lookup(ht, i), (void *)(uintptr_t)i);
    }
    fba_free(ht->entries);
    fba_free(ht);
    return MUNIT_OK;
}

// Shared hash table for concurrency tests
static HashTable *shared_ht;

// Thread function: Insert keys in a range
void *thread_insert(void *arg) {
    uintptr_t thread_id = (uintptr_t)arg;
    for (uint64_t i = thread_id * ENTRIES_PER_THREAD;
         i < (thread_id + 1) * ENTRIES_PER_THREAD; i++) {
        hash_table_insert(shared_ht, i, (void *)i);
    }
    return NULL;
}

// Thread function: Lookup keys in a range
void *thread_lookup(void *arg) {
    uintptr_t thread_id = (uintptr_t)arg;
    for (uint64_t i = thread_id * ENTRIES_PER_THREAD;
         i < (thread_id + 1) * ENTRIES_PER_THREAD; i++) {
        void *result = hash_table_lookup(shared_ht, i);
        munit_assert_ptr_equal(result, (void *)i);
    }
    return NULL;
}

// Thread function: Insert and then delete keys
void *thread_insert_delete(void *arg) {
    uintptr_t thread_id = (uintptr_t)arg;
    for (uint64_t i = thread_id * ENTRIES_PER_THREAD;
         i < (thread_id + 1) * ENTRIES_PER_THREAD; i++) {
        hash_table_insert(shared_ht, i, (void *)i);
        hash_table_remove(shared_ht, i);
    }
    return NULL;
}

// Test: Concurrent insertions
static MunitResult test_concurrent_insert(const MunitParameter params[],
                                          void *user_data) {
    shared_ht = hash_table_create(1);
    pthread_t threads[THREAD_COUNT];
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, thread_insert, (void *)i);
    }
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    for (uint64_t i = 0; i < THREAD_COUNT * ENTRIES_PER_THREAD; i++) {
        munit_assert_ptr_equal(hash_table_lookup(shared_ht, i), (void *)i);
    }
    fba_free(shared_ht->entries);
    fba_free(shared_ht);
    return MUNIT_OK;
}

// Test: Concurrent insertions and lookups
static MunitResult test_concurrent_insert_lookup(const MunitParameter params[],
                                                 void *user_data) {
    shared_ht = hash_table_create(1);
    pthread_t insert_threads[THREAD_COUNT];
    pthread_t lookup_threads[THREAD_COUNT];

    // Launch insertion threads.
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&insert_threads[i], NULL, thread_insert, (void *)i);
    }
    // Wait for all insertion threads to complete.
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_join(insert_threads[i], NULL);
    }

    // Launch lookup threads.
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&lookup_threads[i], NULL, thread_lookup, (void *)i);
    }
    // Wait for all lookup threads to complete.
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_join(lookup_threads[i], NULL);
    }

    fba_free(shared_ht->entries);
    fba_free(shared_ht);
    return MUNIT_OK;
}

// Test: Concurrent insertions and deletions
static MunitResult test_concurrent_insert_delete(const MunitParameter params[],
                                                 void *user_data) {
    shared_ht = hash_table_create(1);
    pthread_t threads[THREAD_COUNT];
    for (uintptr_t i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, thread_insert_delete, (void *)i);
    }
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    for (uint64_t i = 0; i < THREAD_COUNT * ENTRIES_PER_THREAD; i++) {
        munit_assert_null(hash_table_lookup(shared_ht, i));
    }
    fba_free(shared_ht->entries);
    fba_free(shared_ht);
    return MUNIT_OK;
}

// Test suite
static MunitTest hash_table_tests[] = {
        {"/create_destroy", test_create_destroy, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/lookup_non_existent", test_lookup_non_existent, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/insert_after_delete", test_insert_after_delete, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/insert_full_capacity", test_insert_full_capacity, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/tombstone_reuse", test_tombstone_reuse, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/resize_with_deletions", test_resize_with_deletions, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/concurrent/insert", test_concurrent_insert, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/concurrent/insert_lookup", test_concurrent_insert_lookup, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/concurrent/insert_delete", test_concurrent_insert_delete, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite hash_table_suite = {"/hash", hash_table_tests, NULL, 1,
                                            MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&hash_table_suite, NULL, argc, argv);
}
