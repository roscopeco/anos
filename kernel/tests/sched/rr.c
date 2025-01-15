/*
 * Tests for naive round-robin scheduler
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>

#include "fba/alloc.h"
#include "ktypes.h"
#include "mock_pmm.h"
#include "munit.h"
#include "sched.h"
#include "slab/alloc.h"
#include "task.h"

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))

static const int PAGES_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

static const uintptr_t TEST_PAGETABLE_ROOT = 0x1234567887654321;
static const uintptr_t TEST_SYS_SP = 0xc0c010c0a1b2c3d4;
static const uintptr_t TEST_SYS_FUNC = 0x2bad3bad4badf00d;

Task *test_sched_rr_get_runnable_head();

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

    Task *task = test_sched_rr_get_runnable_head();

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks we needed
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
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

    Task *task = test_sched_rr_get_runnable_head();

    // We should have allocated overhead (FBA + Slab), plus a slab for the blocks we needed
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
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

static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)page_area_ptr, 32768);
    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init_zeroes", test_sched_init_zeroes, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_with_ssp", test_sched_init_with_ssp, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_with_all", test_sched_init_with_all, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/sched/rr", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
