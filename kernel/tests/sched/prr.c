/*
 * Tests for prioritised round-robin scheduler
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>

#include "fba/alloc.h"
#include "ktypes.h"
#include "munit.h"
#include "sched.h"
#include "slab/alloc.h"
#include "task.h"
#include "test_pmm.h"

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))

#define INIT_TASK_CLASS TASK_CLASS_NORMAL

static const int PAGES_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

static const uintptr_t TEST_PAGETABLE_ROOT = 0x1234567887654321;
static const uintptr_t TEST_SYS_SP = 0xc0c010c0a1b2c3d4;
static const uintptr_t TEST_SYS_FUNC = 0x2bad3bad4badf00d;

Task *test_sched_prr_get_runnable_head(TaskClass level);
Task *test_sched_prr_set_runnable_head(TaskClass level, Task *task);

uintptr_t get_pagetable_root() { return TEST_PAGETABLE_ROOT; }

static inline void *slab_area_base(void *page_area_ptr) {
    // skip one page used by FBA, and three unused by slab alignment
    return (void *)((uint64_t)page_area_ptr + 0x4000);
}

static MunitResult test_sched_init_zeroes(const MunitParameter params[],
                                          void *page_area_ptr) {
    bool result = sched_init(0, 0, 0);

    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_sched_init_with_ssp(const MunitParameter params[],
                                            void *page_area_ptr) {
    // We must have a sys stack since the routine expects to modify it!
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    bool result = sched_init(0, sys_stack, 0);

    munit_assert_true(result);

    // No realtime tasks
    Task *task = test_sched_prr_get_runnable_head(TASK_CLASS_REALTIME);
    munit_assert_null(task);

    // No high-priority tasks
    task = test_sched_prr_get_runnable_head(TASK_CLASS_HIGH);
    munit_assert_null(task);

    // No idle tasks
    task = test_sched_prr_get_runnable_head(TASK_CLASS_IDLE);
    munit_assert_null(task);

    // Init task in NORMAL class
    task = test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);
    munit_assert_not_null(task);
    munit_assert_null(task->this.next);

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks we needed
    munit_assert_uint32(test_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 2);

    // Process (allocated first) is at the base of the slab area, plus 64 bytes (Slab* is at the base)
    munit_assert_ptr_equal(task->owner, slab_area_base(page_area_ptr) + 64);

    // Task (allocated second) is at the base of the slab area, plus 128 bytes (after the Process)
    munit_assert_ptr_equal(task, slab_area_base(page_area_ptr) + 128);

    munit_assert_uint64(task->owner->pid, ==, 1);
    munit_assert_uint64(task->owner->pml4, ==, TEST_PAGETABLE_ROOT);

    munit_assert_uint64(task->tid, ==, 1);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);

    // Top of kernel stack
    munit_assert_uint64(task->rsp0, ==, sys_stack);

    // Top of kernel stack -128 because reg space was reserved,
    // and func was pushed
    munit_assert_uint64(task->ssp, ==, sys_stack - 128);

    // func is zero, so zero was pushed...
    munit_assert_uint64(*(uint64_t *)task->ssp, ==, 0);

    munit_assert_uint64(task->this.type, ==, KTYPE_TASK);
    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
}

void user_thread_entrypoint(void);

static MunitResult test_sched_init_with_all(const MunitParameter params[],
                                            void *page_area_ptr) {
    // We must have a sys stack since the routine expects to modify it!
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);

    munit_assert_true(result);

    // No realtime tasks
    Task *task = test_sched_prr_get_runnable_head(TASK_CLASS_REALTIME);
    munit_assert_null(task);

    // No high-priority tasks
    task = test_sched_prr_get_runnable_head(TASK_CLASS_HIGH);
    munit_assert_null(task);

    // No idle tasks
    task = test_sched_prr_get_runnable_head(TASK_CLASS_IDLE);
    munit_assert_null(task);

    // Init task in NORMAL class
    task = test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);
    munit_assert_not_null(task);
    munit_assert_null(task->this.next);

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks we needed
    munit_assert_uint32(test_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 2);

    // Process (allocated first) is at the base of the slab area, plus 64 bytes (Slab* is at the base)
    munit_assert_ptr_equal(task->owner, slab_area_base(page_area_ptr) + 64);

    // Task (allocated second) is at the base of the slab area, plus 128 bytes (after the Process)
    munit_assert_ptr_equal(task, slab_area_base(page_area_ptr) + 128);

    munit_assert_uint64(task->owner->pid, ==, 1);
    munit_assert_uint64(task->owner->pml4, ==, TEST_PAGETABLE_ROOT);

    munit_assert_uint64(task->tid, ==, 1);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

    // -128 because reg space was reserved, and func was pushed
    munit_assert_uint64(task->ssp, ==, sys_stack - 128);

    // func addr is "valid" and was pushed after reserved register space...
    munit_assert_uint64(*(uint64_t *)(task->ssp + 120), ==,
                        (uint64_t)user_thread_entrypoint);

    // r15 register slot on stack has user function entrypoint
    munit_assert_uint64(*(uint64_t *)(task->ssp), ==, TEST_SYS_FUNC);

    // r14 register slot on stack has user SP
    munit_assert_uint64(*(uint64_t *)(task->ssp + 8), ==, TEST_SYS_SP);

    munit_assert_uint64(task->this.type, ==, KTYPE_TASK);
    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_no_tasks(const MunitParameter params[],
                                  void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    task_init(NULL);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    test_sched_prr_set_runnable_head(INIT_TASK_CLASS, NULL);

    munit_assert_null(task_current());

    sched_schedule();

    munit_assert_null(task_current());

    return MUNIT_OK;
}

static MunitResult test_sched_schedule_with_no_current_and_one_norm_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task test_task = {0};

    task_init(&test_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Remove the init task that'll have been added...
    test_sched_prr_set_runnable_head(INIT_TASK_CLASS, NULL);

    munit_assert_ptr_equal(task_current(), &test_task);

    sched_schedule();

    munit_assert_ptr_equal(task_current(), &test_task);

    return MUNIT_OK;
}

static void init_task_for_test(Task *task, TaskClass class, uint8_t priority,
                               TaskState state, uint16_t ts_remain) {
    task->state = state;
    task->ts_remain = ts_remain;
    task->class = class;
    task->prio = priority;
}

static MunitResult
test_sched_schedule_with_running_norm_current_and_one_norm_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have one task in the NORMAL queue...
    Task *new_task = test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);

    // ... and a NORMAL task is already running with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the existing task is still running
    munit_assert_ptr_equal(task_current(), &original_task);

    // And the new task is still queued
    munit_assert_ptr_equal(new_task,
                           test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL));

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_expired_norm_current_and_one_norm_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have one task in the NORMAL queue...
    Task *new_task = test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);

    // ... and a NORMAL task is already running with NO time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       0);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the queued task is now running
    munit_assert_ptr_equal(task_current(), new_task);

    // And the original task is now queued
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL),
                           &original_task);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_blocked_norm_current_and_one_norm_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have one task in the NORMAL queue...
    Task *new_task = test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);

    // ... and a NORMAL task is already running, but blocked, with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_BLOCKED,
                       0);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the queued task is now running
    munit_assert_ptr_equal(task_current(), new_task);

    // And the original task is NOT queued (because it's blocked)
    munit_assert_null(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL));

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_running_norm_current_and_one_high_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have no tasks in the NORMAL queue...
    test_sched_prr_set_runnable_head(TASK_CLASS_NORMAL, NULL);

    // And one in the HIGH queue
    Task high_queued_task;
    init_task_for_test(&high_queued_task, TASK_CLASS_HIGH, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_HIGH, &high_queued_task);

    // ... and a NORMAL task is already running with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the queued high-class task is now running
    munit_assert_ptr_equal(task_current(), &high_queued_task);

    // And the original task is now queued
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL),
                           &original_task);

    return MUNIT_OK;
}

static MunitResult test_sched_schedule_with_running_norm_current_and_two_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have one task in the NORMAL queue...
    Task *norm_queued_task =
            test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);

    // And one in the HIGH queue
    Task high_queued_task;
    init_task_for_test(&high_queued_task, TASK_CLASS_HIGH, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_HIGH, &high_queued_task);

    // ... and a NORMAL task is already running with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the queued high-class task is now running
    munit_assert_ptr_equal(task_current(), &high_queued_task);

    // And the original task is now queued at the end, after the norm task that was already there
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL),
                           norm_queued_task);
    munit_assert_ptr_equal(
            test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL)->this.next,
            &original_task);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_running_norm_current_and_two_queued_diff_prio(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have one task in the NORMAL queue...
    Task *norm_queued_task =
            test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL);
    norm_queued_task->prio = 127;

    // And one in the HIGH queue
    Task high_queued_task;
    init_task_for_test(&high_queued_task, TASK_CLASS_HIGH, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_HIGH, &high_queued_task);

    // ... and a NORMAL task is already running with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the queued high-class task is now running
    munit_assert_ptr_equal(task_current(), &high_queued_task);

    // And the original task is now queued before the task with higher prio value
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL),
                           &original_task);
    munit_assert_ptr_equal(
            test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL)->this.next,
            norm_queued_task);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_running_norm_current_and_one_idle_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have no tasks in the NORMAL queue...
    test_sched_prr_set_runnable_head(TASK_CLASS_NORMAL, NULL);

    // And one in the IDLE queue
    Task idle_queued_task;
    init_task_for_test(&idle_queued_task, TASK_CLASS_IDLE, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_IDLE, &idle_queued_task);

    // ... and a NORMAL task is already running with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the original task is still running
    munit_assert_ptr_equal(task_current(), &original_task);

    // And the idle task is still queued
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_IDLE),
                           &idle_queued_task);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_expired_norm_current_and_one_idle_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have no tasks in the NORMAL queue...
    test_sched_prr_set_runnable_head(TASK_CLASS_NORMAL, NULL);

    // And one in the IDLE queue
    Task idle_queued_task;
    init_task_for_test(&idle_queued_task, TASK_CLASS_IDLE, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_IDLE, &idle_queued_task);

    // ... and a NORMAL task is already running with NO time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_RUNNING,
                       0);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the idle task is now running
    munit_assert_ptr_equal(task_current(), &idle_queued_task);

    // The IDLE queue is now empty
    munit_assert_null(test_sched_prr_get_runnable_head(TASK_CLASS_IDLE));

    // And the original task is now queued on NORMAL queue
    munit_assert_ptr_equal(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL),
                           &original_task);

    return MUNIT_OK;
}

static MunitResult
test_sched_schedule_with_blocked_norm_current_and_one_idle_queued(
        const MunitParameter params[], void *page_area_ptr) {
    uintptr_t sys_stack = (uintptr_t)fba_alloc_block();

    Task original_task;
    task_init(&original_task);

    bool result = sched_init(TEST_SYS_SP, sys_stack, TEST_SYS_FUNC);
    munit_assert_true(result);

    // Given we have no tasks in the NORMAL queue...
    test_sched_prr_set_runnable_head(TASK_CLASS_NORMAL, NULL);

    // And one in the IDLE queue
    Task idle_queued_task;
    init_task_for_test(&idle_queued_task, TASK_CLASS_IDLE, 0, TASK_STATE_READY,
                       100);
    test_sched_prr_set_runnable_head(TASK_CLASS_IDLE, &idle_queued_task);

    // ... and a NORMAL task is already running, but blocked, with time left in its slice
    init_task_for_test(&original_task, TASK_CLASS_NORMAL, 0, TASK_STATE_BLOCKED,
                       100);
    munit_assert_ptr_equal(task_current(), &original_task);

    // When we schedule
    sched_schedule();

    // Then the idle task is now running
    munit_assert_ptr_equal(task_current(), &idle_queued_task);

    // The IDLE queue is now empty
    munit_assert_null(test_sched_prr_get_runnable_head(TASK_CLASS_IDLE));

    // And the original task is NOT queued on NORMAL queue (because it's blocked)
    munit_assert_null(test_sched_prr_get_runnable_head(TASK_CLASS_NORMAL));

    return MUNIT_OK;
}

static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)page_area_ptr, 32768);
    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    test_pmm_reset();
}

static MunitTest test_suite_tests[] = {
        // Init
        {(char *)"/init_zeroes", test_sched_init_zeroes, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_with_ssp", test_sched_init_with_ssp, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_with_all", test_sched_init_with_all, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        // No tasks / edge cases
        {(char *)"/sched_no_tasks", test_sched_schedule_with_no_tasks,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/no_current_one_norm_queued",
         test_sched_schedule_with_no_current_and_one_norm_queued, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        // Regular scheduling cases
        {(char *)"/running_norm_current_and_one_norm_queued",
         test_sched_schedule_with_running_norm_current_and_one_norm_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/expired_norm_current_and_one_norm_queued",
         test_sched_schedule_with_expired_norm_current_and_one_norm_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/blocked_norm_current_and_one_norm_queued",
         test_sched_schedule_with_blocked_norm_current_and_one_norm_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/running_norm_current_and_one_high_queued",
         test_sched_schedule_with_running_norm_current_and_one_high_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/running_norm_current_and_two_queued",
         test_sched_schedule_with_running_norm_current_and_two_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/running_norm_current_and_one_idle_queued",
         test_sched_schedule_with_running_norm_current_and_one_idle_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/expired_norm_current_and_one_idle_queued",
         test_sched_schedule_with_expired_norm_current_and_one_idle_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/blocked_norm_current_and_one_idle_queued",
         test_sched_schedule_with_blocked_norm_current_and_one_idle_queued,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        // Different priority handling
        {(char *)"/running_norm_current_and_two_queued_diff_prio",
         test_sched_schedule_with_running_norm_current_and_two_queued_diff_prio,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/sched/prr", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
