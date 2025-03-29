/*
 * Tests for ACPI table initper / parser
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "task.h"
#include "acpitables.h"
#include "fba/alloc.h"
#include "munit.h"
#include "slab/alloc.h"

#include "mock_pmm.h"
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

void kernel_thread_entrypoint(void);
void user_thread_entrypoint(void);

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

    // TaskSched (allocated first) is at the base of the slab area, plus 64 bytes
    munit_assert_ptr_equal(task->sched, slab_area_base(page_area_ptr) + 64);

    // Task (allocated second) is at the base of the slab area, plus 128 bytes
    munit_assert_ptr_equal(task, slab_area_base(page_area_ptr) + 128);

    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

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

    // TaskSched (allocated first) is at the base of the slab area, plus 64 bytes
    munit_assert_ptr_equal(task->sched, slab_area_base(page_area_ptr) + 64);

    // Task (allocated second) is at the base of the slab area, plus 128 bytes
    munit_assert_ptr_equal(task, slab_area_base(page_area_ptr) + 128);

    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
    munit_assert_uint64(task->pml4, ==, TEST_PAGETABLE_ROOT);
    munit_assert_uint64(task->rsp0, ==, sys_stack);

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

    // TaskSched (allocated first) is at the base of the slab area, plus 64 bytes
    munit_assert_ptr_equal(task->sched, slab_area_base(page_area_ptr) + 64);

    // Task (allocated second) is at the base of the slab area, plus 128 bytes
    munit_assert_ptr_equal(task, slab_area_base(page_area_ptr) + 128);

    munit_assert_ptr_equal(task->owner, &mock_owner);

    munit_assert_uint64(task->sched->tid, ==, 2);
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

    munit_assert_ptr(task->this.next, ==, NULL);

    return MUNIT_OK;
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

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/tast", test_suite_tests, NULL,
                                      1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
