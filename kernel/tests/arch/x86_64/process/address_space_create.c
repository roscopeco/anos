/*
 * stage3 - Process address space initialisation test
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO these tests suck, need to cover the set-up of the 
 * shared regions and stack!
 */

#include "munit.h"

#include "mock_machine.h"
#include "mock_pmm.h"
#include "mock_recursive.h"
#include "pmm/pagealloc.h"
#include "spinlock.h"
#include "vmm/vmmapper.h"

#include "process/address_space.h"
#include "smp/state.h"

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
        complete_pml4.entries[i] = 0xA000 + i | PG_PRESENT | PG_WRITE;
    }

    // Set up recursive mapping in complete_pml4
    complete_pml4.entries[RECURSIVE_ENTRY] =
            (uintptr_t)&complete_pml4 | PG_PRESENT | PG_WRITE;

    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);

    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
}

static MunitResult test_create_success(const MunitParameter params[],
                                       void *fixture) {
    // Given
    complete_pml4.entries[RECURSIVE_ENTRY_OTHER] = 0x1234 | PG_PRESENT;

    // When
    uintptr_t result =
            address_space_create(0x0, 0x0, 0, (void *)0, 0, (void *)0);

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
                        ((uintptr_t)mock_new_pml4 | PG_WRITE | PG_PRESENT));

    // Verify other recursive entry is zeroed
    munit_assert_uint64(mock_new_pml4->entries[RECURSIVE_ENTRY_OTHER], ==, 0);

    // Verify kernel space was copied
    for (int i = KERNEL_BEGIN_ENTRY; i < 512; i++) {
        munit_assert_uint64(mock_new_pml4->entries[i], ==,
                            complete_pml4.entries[i]);
    }

    // Verify original PML4 was restored
    munit_assert_uint64(complete_pml4.entries[RECURSIVE_ENTRY_OTHER], ==,
                        0x1234 | PG_PRESENT);

    return MUNIT_OK;
}

static MunitResult test_allocation_failure(const MunitParameter params[],
                                           void *fixture) {
    for (int i = 0; i < MOCK_PMM_MAX_PAGES; i++) {
        page_alloc(physical_region); // All pages are allocated...
    }

    const uintptr_t result =
            address_space_create(0x0, 0x0, 0, (void *)0, 0, (void *)0);

    munit_assert_uint64(result, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_stack_in_kernel_space(const MunitParameter params[],
                                              void *data) {
#ifdef CONSERVATIVE_BUILD
    const uintptr_t result = address_space_create(VM_KERNEL_SPACE_START, 0x1000,
                                                  0, NULL, 0, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult
test_stack_value_count_too_big_for_stack(const MunitParameter params[],
                                         void *data) {
#ifdef CONSERVATIVE_BUILD
    const uintptr_t result = address_space_create(
            0x0, 0x1000, 0, NULL, (0x1000 / sizeof(uintptr_t)) + 1, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult
test_stack_value_count_too_big_absolute(const MunitParameter params[],
                                        void *data) {
#ifdef CONSERVATIVE_BUILD
    const uintptr_t result =
            address_space_create(0x0, MAX_STACK_VALUE_COUNT * sizeof(uintptr_t),
                                 0, NULL, MAX_STACK_VALUE_COUNT + 1, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult test_region_start_misaligned(const MunitParameter params[],
                                                void *data) {
#ifdef CONSERVATIVE_BUILD
    AddressSpaceRegion region = {.start = 0x1003, .len_bytes = 0x1000};
    const uintptr_t result =
            address_space_create(0x0, 0x1000, 1, &region, 0, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult test_region_length_misaligned(const MunitParameter params[],
                                                 void *data) {
#ifdef CONSERVATIVE_BUILD
    AddressSpaceRegion region = {.start = 0x1000, .len_bytes = 0x123};
    const uintptr_t result =
            address_space_create(0x0, 0x1000, 1, &region, 0, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult
test_region_exceeds_kernel_start(const MunitParameter params[], void *data) {
#ifdef CONSERVATIVE_BUILD
    AddressSpaceRegion region = {.start = VM_KERNEL_SPACE_START - 0x800,
                                 .len_bytes = 0x1000};
    const uintptr_t result =
            address_space_create(0x0, 0x1000, 1, &region, 0, NULL);

    munit_assert_uint64(result, ==, 0);
#endif
    return MUNIT_OK;
}

static MunitResult test_stack_values_copied(const MunitParameter params[],
                                            void *data) {
    const uint64_t values[4] = {0xDEAD, 0xBEEF, 0xFEED, 0xFACE};
    const uintptr_t result =
            address_space_create(0x1000, 0x2000, 0, NULL, 4, values);

    munit_assert_not_null((void *)result);

    const uint64_t *mock_stacked_page = (uint64_t *)mock_cpu_temp_page;

    munit_assert_uint64(mock_stacked_page[511], ==, 0xFACE);
    munit_assert_uint64(mock_stacked_page[510], ==, 0xFEED);
    munit_assert_uint64(mock_stacked_page[509], ==, 0xBEEF);
    munit_assert_uint64(mock_stacked_page[508], ==, 0xDEAD);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {"/success", test_create_success, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc_failure", test_allocation_failure, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/stack_in_kernel_space", test_stack_in_kernel_space,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/stack_value_count_too_big_for_stack",
         test_stack_value_count_too_big_for_stack, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/stack_value_count_too_big_absolute",
         test_stack_value_count_too_big_absolute, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/region_start_misaligned", test_region_start_misaligned,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/region_length_misaligned",
         test_region_length_misaligned, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/region_exceeds_kernel_start",
         test_region_exceeds_kernel_start, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_space/stack_values_copied", test_stack_values_copied,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {"/address_space/create", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}