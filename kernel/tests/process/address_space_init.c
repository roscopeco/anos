/*
 * stage3 - Process address space initialisation test
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "munit.h"

#include "mock_pagetables.h"
#include "mock_pmm.h"
#include "mock_vmm.h"
#include "vmm/vmmapper.h"

#include "process/address_space.h"
#include "smp/state.h"

uint32_t refcount_map_increment(uintptr_t addr) { return 1; }

uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys, uint64_t flags) {
    return ((phys & ~0xFFF) >> 2) | flags;
}

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))
static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);

    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
}

static MunitResult test_init_success(const MunitParameter params[],
                                     void *fixture) {
    // Test successful initialization
    const bool result = address_space_init();

    munit_assert_true(result);

    // Verify PML4 entries are properly set up
    for (int i = FIRST_KERNEL_PML4E + 2; i < 512; i++) {
        munit_assert_not_null((void *)complete_pml4.entries[i]);
        munit_assert((complete_pml4.entries[i] & PG_PRESENT) != 0);
        munit_assert((complete_pml4.entries[i] & PG_WRITE) != 0);
    }

    return MUNIT_OK;
}

static MunitResult
test_init_with_existing_entries(const MunitParameter params[], void *fixture) {
    // Set up some pre-existing entries
    complete_pml4.entries[FIRST_KERNEL_PML4E + 3] = 0x1000 | PG_PRESENT;
    complete_pml4.entries[FIRST_KERNEL_PML4E + 4] =
            0x2000 | PG_PRESENT | PG_WRITE;

    bool result = address_space_init();
    munit_assert_true(result);

    // Verify pre-existing entries weren't modified
    munit_assert_uint64(complete_pml4.entries[FIRST_KERNEL_PML4E + 3], ==,
                        0x1000 | PG_PRESENT);
    munit_assert_uint64(complete_pml4.entries[FIRST_KERNEL_PML4E + 4], ==,
                        0x2000 | PG_PRESENT | PG_WRITE);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {"/success", test_init_success, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/existing_entries", test_init_with_existing_entries, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/address_space/init", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}