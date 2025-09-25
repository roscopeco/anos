/*
 * Tests for spinlocks
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include "munit.h"
#include "spinlock.h"

#define THREAD_COUNT 10
#define THREAD_NUM_COUNT 256

static uint64_t thread_nums[THREAD_NUM_COUNT];

static MunitResult test_spinlock_init(const MunitParameter params[], void *param) {
    SpinLock lock = {0xffffffffffffffff};

    spinlock_init(&lock);

    munit_assert_int64(lock.lock, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_spinlock_lock_unlock(const MunitParameter params[], void *param) {
    SpinLock lock = {0x0, 0x0};

    spinlock_lock(&lock);

    munit_assert_int64(lock.lock, ==, 1);

    spinlock_unlock(&lock);

    munit_assert_int64(lock.lock, ==, 0);

    return MUNIT_OK;
}

static void *spinlock_thread_func(void *arg) {
    SpinLock *lock = (SpinLock *)arg;
    uint64_t thread_id = (uint64_t)pthread_self();

    spinlock_lock(lock);
    for (int i = 0; i < THREAD_NUM_COUNT; i++) {
        thread_nums[i] = thread_id;
        usleep(500);
    }
    spinlock_unlock(lock);

    return NULL;
}

static MunitResult test_spinlock_multithreaded(const MunitParameter params[], void *param) {
    SpinLock lock = {0x0};
    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, spinlock_thread_func, &lock);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    munit_assert_uint64(lock.lock, ==, 0);
    munit_assert_uint64(thread_nums[0], !=, 0);
    for (int i = 1; i < THREAD_NUM_COUNT; i++) {
        munit_assert_uint64(thread_nums[i], ==, thread_nums[0]);
    }

    return MUNIT_OK;
}

static MunitResult test_spinlock_reentrant_init(const MunitParameter params[], void *param) {
    ReentrantSpinLock lock = {0xffffffffffffffff, 0xffffffffffffffff};

    spinlock_reentrant_init(&lock);

    munit_assert_int64(lock.lock, ==, 0);
    munit_assert_int64(lock.ident, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_spinlock_reentrant_lock_unlock(const MunitParameter params[], void *param) {
    bool result;
    ReentrantSpinLock lock = {0x0, 0x0};

    // Can lock
    result = spinlock_reentrant_lock(&lock, 0x1234);

    munit_assert_true(result);
    munit_assert_int64(lock.lock, ==, 1);
    munit_assert_int64(lock.ident, ==, 0x1234);

    // Can retake the lock
    result = spinlock_reentrant_lock(&lock, 0x1234);

    munit_assert_false(result);
    munit_assert_int64(lock.lock, ==, 1);
    munit_assert_int64(lock.ident, ==, 0x1234);

    // Can't unlock with another ident
    result = spinlock_reentrant_unlock(&lock, 0x5678);

    munit_assert_false(result);
    munit_assert_int64(lock.lock, ==, 1);
    munit_assert_int64(lock.ident, ==, 0x1234);

    // Can unlock with original ident
    result = spinlock_reentrant_unlock(&lock, 0x1234);

    munit_assert_true(result);
    munit_assert_int64(lock.lock, ==, 0);
    munit_assert_int64(lock.ident, ==, 0);

    return MUNIT_OK;
}

static void *spinlock_reentrant_thread_func(void *arg) {
    ReentrantSpinLock *lock = (ReentrantSpinLock *)arg;
    uint64_t thread_id = (uint64_t)pthread_self();

    spinlock_reentrant_lock(lock, thread_id);
    for (int i = 0; i < THREAD_NUM_COUNT; i++) {
        // may as well test we can retake the lock here too...
        spinlock_reentrant_lock(lock, thread_id);

        thread_nums[i] = thread_id;
        usleep(500);
    }
    spinlock_reentrant_unlock(lock, thread_id);

    return NULL;
}

static MunitResult test_reentrant_multithreaded(const MunitParameter params[], void *param) {
    ReentrantSpinLock lock = {0x0, 0x0};
    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, spinlock_reentrant_thread_func, &lock);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    munit_assert_uint64(lock.lock, ==, 0);
    munit_assert_uint64(thread_nums[0], !=, 0);
    for (int i = 1; i < THREAD_NUM_COUNT; i++) {
        munit_assert_uint64(thread_nums[i], ==, thread_nums[0]);
    }

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init", test_spinlock_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/lock_unlock", test_spinlock_lock_unlock, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/spinlock_multithreaded", test_spinlock_multithreaded, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/reentrant_init", test_spinlock_reentrant_init, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/reentrant_lock_unlock", test_spinlock_reentrant_lock_unlock, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/reentrant_multithreaded", test_reentrant_multithreaded, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/spinlock", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}