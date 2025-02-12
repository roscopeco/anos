/*
 * stage3 - Process address space initialisation test
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "munit.h"

#include "mock_machine.h"
#include "mock_pmm.h"
#include "mock_recursive.h"
#include "pmm/pagealloc.h"
#include "spinlock.h"
#include "vmm/vmmapper.h"

#include "process/address_space.h"

extern MemoryRegion *physical_region;

#define TEST_PAGE_COUNT ((32768))

// External declarations for mock tables from mock_recursive.h
extern PageTable empty_pml4;
extern PageTable complete_pml4;
extern PageTable complete_pdpt;
extern PageTable complete_pd;
extern PageTable complete_pt;

static void *test_setup(const MunitParameter params[], void *user_data) {
    // Reset complete_pml4 to a known state
    memset(&complete_pml4, 0, sizeof(PageTable));

    // Set up kernel space entries in complete_pml4
    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        complete_pml4.entries[i] = 0xA000 + i | PRESENT | WRITE;
    }

    // Set up recursive mapping in complete_pml4
    complete_pml4.entries[RECURSIVE_ENTRY] =
            (uintptr_t)&complete_pml4 | PRESENT | WRITE;

    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);

    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    munit_assert_uint32(mock_machine_intr_disable_level(), ==, 0);

    free(page_area_ptr);
    mock_pmm_reset();
}

static MunitResult test_create_success(const MunitParameter params[],
                                       void *fixture) {
    // Given
    complete_pml4.entries[RECURSIVE_ENTRY_OTHER] = 0x1234 | PRESENT;

    // When
    uintptr_t result = address_space_create();

    // Then.....
    munit_assert_not_null((void *)result);

    PageTable *mock_new_pml4 = (PageTable *)result;

    // Verify userspace is zeroed
    for (int i = 0; i < RECURSIVE_ENTRY; i++) {
#ifdef DEBUG_ADDRESS_SPACE_CREATE_COPY_ALL
        munit_assert_uint64(mock_new_pml4.entries[i], ==,
                            complete_pml4.entries[i]);
#else
        munit_assert_uint64(mock_new_pml4->entries[i], ==, 0);
#endif
    }

    // Verify recursive entry
    munit_assert_uint64(mock_new_pml4->entries[RECURSIVE_ENTRY], ==,
                        ((uintptr_t)mock_new_pml4 | WRITE | PRESENT));

    // Verify other recursive entry is zeroed
    munit_assert_uint64(mock_new_pml4->entries[RECURSIVE_ENTRY_OTHER], ==, 0);

    // Verify kernel space was copied
    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        munit_assert_uint64(mock_new_pml4->entries[i], ==,
                            complete_pml4.entries[i]);
    }

    // Verify original PML4 was restored
    munit_assert_uint64(complete_pml4.entries[RECURSIVE_ENTRY_OTHER], ==,
                        0x1234 | PRESENT);

    // Verify we disabled interrupts
    munit_assert_uint32(mock_machine_max_intr_disable_level(), ==, 1);

    return MUNIT_OK;
}

static MunitResult test_allocation_failure(const MunitParameter params[],
                                           void *fixture) {
    for (int i = 0; i < MOCK_PMM_MAX_PAGES; i++) {
        page_alloc(physical_region); // All pages are allocated...
    }

    uintptr_t result = address_space_create();
    munit_assert_uint64(result, ==, 0);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {"/success", test_create_success, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc_failure", test_allocation_failure, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/address_space/create", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}