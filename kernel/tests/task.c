/*
 * Tests for ACPI table initper / parser
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "munit.h"

#include "fba/alloc.h"
#include "slab/alloc.h"
#include "task.h"

#include "mock_pmm.h"
#include "mock_recursive.h"
#include "smp/state.h"

static const int PAGES_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

static const uintptr_t TEST_PAGETABLE_ROOT = 0x1234567887654321;
static const uintptr_t TEST_SYS_SP = 0xc0c010c0a1b2c3d4;
static const uintptr_t TEST_SYS_FUNC = 0x2bad3bad4badf00d;
static const uintptr_t TEST_BOOT_FUNC = 0x1010101020101020;
static const uintptr_t TEST_TASK_CLASS = TASK_CLASS_NORMAL;
static const void *TEST_TASK_TSS = (void *)0x9090404080803030;

static uintptr_t sys_stack;
static Process mock_owner;

static char last_konservative_msg[128];
static bool panic_called = false;

void mock_kprintf(const char *msg) {
    strncpy(last_konservative_msg, msg, sizeof(last_konservative_msg));
}

void kernel_thread_entrypoint(void);
void user_thread_entrypoint(void);

void panic_sloc(char *msg) { panic_called = true; }
void process_destroy(Process *process) { /* nothing*/ }
void sched_schedule(void) { /* nothing*/ }

static inline void *slab_area_base(void *page_area_ptr) {
    // skip one page used by FBA, and three unused by slab alignment
    return (void *)((uint64_t)page_area_ptr + 0x4000);
}

static MunitResult test_task_create_new(const MunitParameter params[],
                                        void *page_area_ptr) {
    Task *task =
            task_create_new(&mock_owner, TEST_SYS_SP, sys_stack, TEST_BOOT_FUNC,
                            TEST_SYS_FUNC, TASK_CLASS_IDLE);

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks
    // we needed, as well as one FBA for the task data block.
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 3);

    // Task is at the base of the FBA area, plus 8KiB bytes (for FBA overhead)
    munit_assert_ptr_equal(task, page_area_ptr + 0x2000);

    // Task sched is actually the ssched member in the 4KiB task struct...
    munit_assert_ptr_equal(task->sched, &task->ssched);

    // Task data is actually the sdata member in the 4KiB task struct...
    munit_assert_ptr_equal(task->data, &task->sdata);

    // Check basic details
    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

    munit_assert_uint8(task->sched->state, ==, TASK_STATE_READY);
    munit_assert_uint8(task->sched->status_flags, ==, 0);

    // -128 because reg space was reserved, and func was pushed
    munit_assert_uint64(task->ssp, ==, sys_stack - 128);

    // func addr is "valid" and was pushed after reserved register space...
    munit_assert_uint64(*(uint64_t *)(task->ssp + 120), ==,
                        (uint64_t)TEST_BOOT_FUNC);

    // r15 register slot on stack has user function entrypoint
    munit_assert_uint64(*(uint64_t *)(task->ssp), ==, TEST_SYS_FUNC);

    // r14 register slot on stack has user SP
    munit_assert_uint64(*(uint64_t *)(task->ssp + 8), ==, TEST_SYS_SP);

    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
}

static MunitResult test_task_create_kernel(const MunitParameter params[],
                                           void *page_area_ptr) {
    Task *task = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                    TEST_SYS_FUNC, TASK_CLASS_IDLE);

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks
    // we needed, as well as one FBA for the task data block.
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 3);

    // Task is at the base of the FBA area, plus 8KiB bytes (for FBA overhead)
    munit_assert_ptr_equal(task, page_area_ptr + 0x2000);

    // Task sched is actually the ssched member in the 4KiB task struct...
    munit_assert_ptr_equal(task->sched, &task->ssched);

    // Task data is actually the sdata member in the 4KiB task struct...
    munit_assert_ptr_equal(task->data, &task->sdata);

    // Check basic details
    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

    munit_assert_uint8(task->sched->state, ==, TASK_STATE_READY);
    munit_assert_uint8(task->sched->status_flags, ==, 0);

    // -128 because reg space was reserved, and func was pushed
    munit_assert_uint64(task->ssp, ==, sys_stack - 128);

    // func addr is "valid" and was pushed after reserved register space...
    munit_assert_uint64(*(uint64_t *)(task->ssp + 120), ==,
                        (uint64_t)kernel_thread_entrypoint);

    // r15 register slot on stack has user function entrypoint
    munit_assert_uint64(*(uint64_t *)(task->ssp), ==, TEST_SYS_FUNC);

    // r14 register slot on stack has user SP
    munit_assert_uint64(*(uint64_t *)(task->ssp + 8), ==, TEST_SYS_SP);

    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
}

static MunitResult test_task_create_user(const MunitParameter params[],
                                         void *page_area_ptr) {
    Task *task = task_create_user(&mock_owner, TEST_SYS_SP, sys_stack,
                                  TEST_SYS_FUNC, TASK_CLASS_IDLE);

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks
    // we needed, as well as one FBA for the task data block.
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 3);

    // Task is at the base of the FBA area, plus 8KiB bytes (for FBA overhead)
    munit_assert_ptr_equal(task, page_area_ptr + 0x2000);

    // Task sched is actually the ssched member in the 4KiB task struct...
    munit_assert_ptr_equal(task->sched, &task->ssched);

    // Task data is actually the sdata member in the 4KiB task struct...
    munit_assert_ptr_equal(task->data, &task->sdata);

    // Check basic details
    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

    munit_assert_uint8(task->sched->state, ==, TASK_STATE_READY);
    munit_assert_uint8(task->sched->status_flags, ==, 0);

    // -128 because reg space was reserved, and func was pushed
    munit_assert_uint64(task->ssp, ==, sys_stack - 128);

    // func addr is "valid" and was pushed after reserved register space...
    munit_assert_uint64(*(uint64_t *)(task->ssp + 120), ==,
                        (uint64_t)user_thread_entrypoint);

    // r15 register slot on stack has user function entrypoint
    munit_assert_uint64(*(uint64_t *)(task->ssp), ==, TEST_SYS_FUNC);

    // r14 register slot on stack has user SP
    munit_assert_uint64(*(uint64_t *)(task->ssp + 8), ==, TEST_SYS_SP);

    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
}

static MunitResult test_task_destroy_success(const MunitParameter params[],
                                             void *page_area_ptr) {
    Task *task = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                    TEST_SYS_FUNC, TASK_CLASS_IDLE);
    task->sched->state = TASK_STATE_TERMINATED;

    task_destroy(task);

    // only one actual page free (from fba_free) - the rest are slab...
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, 1);

    return MUNIT_OK;
}

static MunitResult
test_task_destroy_null_sched_or_data(const MunitParameter params[],
                                     void *page_area_ptr) {
#ifdef CONSERVATIVE_BUILD
    Task *task = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                    TEST_SYS_FUNC, TASK_CLASS_IDLE);
    task->sched->state = TASK_STATE_TERMINATED;

    // Null sched
    task->sched = NULL;
    task_destroy(task);

    munit_assert_memory_equal(32, last_konservative_msg,
                              "[BUG] Destroy task with NULL sched");

    // Null data
    Task *task2 = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                     TEST_SYS_FUNC, TASK_CLASS_IDLE);
    task2->sched->state = TASK_STATE_TERMINATED;
    task2->data = NULL;
    task_destroy(task2);
    munit_assert_memory_equal(32, last_konservative_msg,
                              "[BUG] Destroy task with NULL data area");

    return MUNIT_OK;
#else
    return MUNIT_SKIP;
#endif
}

static MunitResult test_task_destroy_wrong_state(const MunitParameter params[],
                                                 void *page_area_ptr) {
#ifdef CONSERVATIVE_BUILD
    Task *task = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                    TEST_SYS_FUNC, TASK_CLASS_IDLE);
    task->sched->state = TASK_STATE_RUNNING;

    task_destroy(task);

    munit_assert_true(panic_called);
    return MUNIT_OK;
#else
    return MUNIT_SKIP;
#endif
}

static MunitResult
test_task_remove_from_process_success(const MunitParameter params[],
                                      void *page_area_ptr) {
    Task *task = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                    TEST_SYS_FUNC, TASK_CLASS_IDLE);
    ProcessTask *link = slab_alloc_block();
    link->task = task;
    link->this.next = NULL;
    task->owner->tasks = link;

    task_remove_from_process(task);
    munit_assert_ptr_null(mock_owner.tasks);

    return MUNIT_OK;
}

static MunitResult
test_task_remove_from_process_not_found(const MunitParameter params[],
                                        void *page_area_ptr) {
    Task *task1 = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                     TEST_SYS_FUNC, TASK_CLASS_IDLE);
    Task *task2 = task_create_kernel(&mock_owner, TEST_SYS_SP, sys_stack,
                                     TEST_SYS_FUNC, TASK_CLASS_IDLE);

    ProcessTask *link = slab_alloc_block();
    link->task = task1;
    link->this.next = NULL;
    task1->owner->tasks = link;

    task_remove_from_process(task2);

    // Still exists
    munit_assert_ptr_equal(mock_owner.tasks, link);
    return MUNIT_OK;
}

static MunitResult
test_task_remove_from_process_null_inputs(const MunitParameter params[],
                                          void *page_area_ptr) {
    task_remove_from_process(NULL);

    Task dummy_task = {0};
    task_remove_from_process(&dummy_task);

    return MUNIT_OK;
}

static MunitResult test_task_destroy_null_task(const MunitParameter params[],
                                               void *page_area_ptr) {
#ifdef CONSERVATIVE_BUILD
    task_destroy(NULL);
    munit_assert_memory_equal(32, last_konservative_msg,
                              "[BUG] Destroy task with NULL task");
    return MUNIT_OK;
#else
    return MUNIT_SKIP;
#endif
}

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))
static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)page_area_ptr, 32768);

    sys_stack = (uintptr_t)fba_alloc_block();

    task_init((void *)TEST_TASK_TSS);

    mock_owner.pml4 = TEST_PAGETABLE_ROOT;

    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
}

static MunitTest test_suite_tests[] = {
        // Init
        {(char *)"/create_new", test_task_create_new, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/create_kernel", test_task_create_kernel, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/create_user", test_task_create_user, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/destroy_success", test_task_destroy_success, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/destroy_null_sched_or_data",
         test_task_destroy_null_sched_or_data, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/destroy_wrong_state", test_task_destroy_wrong_state,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/destroy_null_task", test_task_destroy_null_task, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/remove_from_process_success",
         test_task_remove_from_process_success, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/remove_from_process_not_found",
         test_task_remove_from_process_not_found, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/remove_from_process_null",
         test_task_remove_from_process_null_inputs, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/task", test_suite_tests, NULL,
                                      1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
