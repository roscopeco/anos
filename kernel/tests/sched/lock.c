/*
 * Tests for scheduler locks
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "mock_machine.h"
#include "mock_spinlock.h"
#include "munit.h"
#include "sched.h"
#include "smp/state.h"

static MunitResult
test_sched_lock_this_cpu_unlocked(const MunitParameter params[], void *param) {
    uint64_t flags = sched_lock_this_cpu();

    munit_assert_uint32(mock_spinlock_get_lock_count(), ==, 1);
    munit_assert_uint32(mock_spinlock_get_unlock_count(), ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_sched_lock_this_cpu_locked(const MunitParameter params[], void *param) {
    uint64_t flags = sched_lock_this_cpu();

    munit_assert_uint32(mock_spinlock_get_lock_count(), ==, 1);
    munit_assert_uint32(mock_spinlock_get_unlock_count(), ==, 0);

    flags = sched_lock_this_cpu(); // In reality this would deadlock

    // still only one lock, it's non-reentrant!
    munit_assert_uint32(mock_spinlock_get_lock_count(), ==, 1);
    munit_assert_uint32(mock_spinlock_get_unlock_count(), ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_sched_unlock_this_cpu_locked(const MunitParameter params[], void *param) {
    uint64_t flags = sched_lock_this_cpu();

    sched_unlock_this_cpu(flags);

    munit_assert_uint32(mock_spinlock_get_lock_count(), ==, 1);
    munit_assert_uint32(mock_spinlock_get_unlock_count(), ==, 1);

    return MUNIT_OK;
}

static MunitResult
test_sched_unlock_this_cpu_unlocked(const MunitParameter params[],
                                    void *param) {
    sched_unlock_this_cpu(0x200);

    munit_assert_uint32(mock_spinlock_get_lock_count(), ==, 0);
    munit_assert_uint32(mock_spinlock_get_unlock_count(), ==, 1);

    munit_assert_uint32(mock_machine_intr_disable_level(), ==, 0);
    munit_assert_uint32(mock_machine_max_intr_disable_level(), ==, 0);

    return MUNIT_OK;
}

static void *test_setup(const MunitParameter params[], void *user_data) {
    mock_spinlock_reset();
    return NULL;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/lock_unlocked", test_sched_lock_this_cpu_unlocked,
         test_setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/lock_locked", test_sched_lock_this_cpu_locked, test_setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/unlock_locked", test_sched_unlock_this_cpu_locked,
         test_setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/unlock_unlocked", test_sched_unlock_this_cpu_unlocked,
         test_setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/sched/lock", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
