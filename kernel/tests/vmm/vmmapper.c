/*
 * Tests for the virtual memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "vmm/vmmapper.h"
#include "mock_pmm.h"
#include "munit.h"

// must include after munit.h!
#include "mock_recursive.h"

static MunitResult test_map_page_empty_pml4_0(const MunitParameter params[],
                                              void *param) {
    vmm_map_page_in(&empty_pml4, 0x0, 0x1000, 0);

    munit_assert_uint64(empty_pml4.entries[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4.entries[0] & 0xFFFFFFFFFFFFF000);
    munit_assert_uint64(pdpt[0], !=, 0);
    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pdpt[i], ==, 0);
    }

    // pd was created and mapped
    uint64_t *pd = (uint64_t *)(pdpt[0] & 0xFFFFFFFFFFFFF000);
    munit_assert_uint64(pd[0], !=, 0);
    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pd[i], ==, 0);
    }

    // pt was created and mapped
    uint64_t *pt = (uint64_t *)(pd[0] & 0xFFFFFFFFFFFFF000);
    munit_assert_uint64(pt[0], !=, 0);
    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pt[i], ==, 0);
    }

    // Correct page was mapped
    munit_assert_uint64(pt[0], ==, 0x1000);

    // We allocated three pages (one for each table level)
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_2M(const MunitParameter params[],
                                               void *param) {
    vmm_map_page_in(&empty_pml4, 0x200000, 0x1000, 0);

    munit_assert_uint64(empty_pml4.entries[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4.entries[0] & 0xFFFFFFFFFFFFF000);
    munit_assert_uint64(pdpt[0], !=, 0);
    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pdpt[i], ==, 0);
    }

    // pd was created and mapped
    uint64_t *pd = (uint64_t *)(pdpt[0] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pd[0], ==, 0);
    munit_assert_uint64(pd[1], !=, 0);

    for (int i = 2; i < 512; i++) {
        munit_assert_uint64(pd[i], ==, 0);
    }

    // pt was created and mapped at pde1
    uint64_t *pt = (uint64_t *)(pd[1] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pt[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pt[i], ==, 0);
    }

    // Correct page was mapped
    munit_assert_uint64(pt[0], ==, 0x1000);

    // We allocated three pages (one for each table level)
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_1G(const MunitParameter params[],
                                               void *param) {
    vmm_map_page_in(&empty_pml4, 0x40000000, 0x1000, 0);

    munit_assert_uint64(empty_pml4.entries[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4.entries[0] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pdpt[0], ==, 0);
    munit_assert_uint64(pdpt[1], !=, 0);

    for (int i = 2; i < 512; i++) {
        munit_assert_uint64(pdpt[i], ==, 0);
    }

    // pd was created and mapped
    uint64_t *pd = (uint64_t *)(pdpt[1] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pd[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pd[i], ==, 0);
    }

    // pt was created and mapped at pd0
    uint64_t *pt = (uint64_t *)(pd[0] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pt[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pt[i], ==, 0);
    }

    // Correct page was mapped
    munit_assert_uint64(pt[0], ==, 0x1000);

    // We allocated three pages (one for each table level)
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_512G(const MunitParameter params[],
                                                 void *param) {
    vmm_map_page_in(&empty_pml4, 0x8000000000, 0x1000, 0);

    munit_assert_uint64(empty_pml4.entries[0], ==, 0);
    munit_assert_uint64(empty_pml4.entries[1], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4.entries[1] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pdpt[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pdpt[i], ==, 0);
    }

    // pd was created and mapped
    uint64_t *pd = (uint64_t *)(pdpt[0] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pd[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pd[i], ==, 0);
    }

    // pt was created and mapped at pd0
    uint64_t *pt = (uint64_t *)(pd[0] & 0xFFFFFFFFFFFFF000);

    munit_assert_uint64(pt[0], !=, 0);

    for (int i = 1; i < 512; i++) {
        munit_assert_uint64(pt[i], ==, 0);
    }

    // Correct page was mapped
    munit_assert_uint64(pt[0], ==, 0x1000);

    // We allocated three pages (one for each table level)
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_complete_pml4_0(const MunitParameter params[],
                                                 void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x1000);

    vmm_map_page_in(&complete_pml4, 0x0, 0x1000, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt.entries[0], ==, 0x1000);

    // No pages were allocated
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_map_page_complete_pml4_phys_4G(const MunitParameter params[],
                                    void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x100000000);

    vmm_map_page_in(&complete_pml4, 0x0, 0x100000000, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt.entries[0], ==, 0x100000000);

    // No pages were allocated
    munit_assert_uint8(mock_pmm_get_total_page_allocs(), ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_map_page_containing_already(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x1000);

    bool result = vmm_map_page_containing_in((uint64_t *)&complete_pml4, 0x0,
                                             0x1000, 0);
    munit_assert_true(result);

    // Correct page was mapped
    munit_assert_uint64(complete_pt.entries[0], ==, 0x1000);

    return MUNIT_OK;
}

static MunitResult
test_map_page_containing_within(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x1000);

    bool result = vmm_map_page_containing_in((uint64_t *)&complete_pml4, 0x0,
                                             0x1234, 0);
    munit_assert_true(result);

    // Correct page was mapped
    munit_assert_uint64(complete_pt.entries[0], ==, 0x1000);

    return MUNIT_OK;
}

static MunitResult test_unmap_page_empty_pml4_0(const MunitParameter params[],
                                                void *param) {
    vmm_unmap_page_in((uint64_t *)&empty_pml4, 0x0);

    munit_assert_uint64(empty_pml4.entries[0], ==, 0);

    return MUNIT_OK;
}

static MunitResult test_unmap_page_empty_pml4_2M(const MunitParameter params[],
                                                 void *param) {
    vmm_unmap_page_in((uint64_t *)&empty_pml4, 0x200000);

    munit_assert_uint64(empty_pml4.entries[0], ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_unmap_page_complete_pml4_0(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x1000);

    vmm_map_page_in(&complete_pml4, 0x0, 0x1000, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt.entries[0], ==, 0x1000);

    uintptr_t unmapped_phys =
            vmm_unmap_page_in((uint64_t *)&complete_pml4, 0x0);

    // Higher-level tables are untouched
    munit_assert_uint64(complete_pml4.entries[0], ==,
                        (uint64_t)&complete_pdpt | PRESENT);
    munit_assert_uint64(complete_pdpt.entries[0], ==,
                        (uint64_t)&complete_pd | PRESENT);
    munit_assert_uint64(complete_pd.entries[0], ==,
                        (uint64_t)&complete_pt | PRESENT);

    // Correct page was unmapped in PT
    munit_assert_uint64(complete_pt.entries[0], ==, 0);

    // Physical address of previously-mapped page was returned
    munit_assert_uint64(unmapped_phys, ==, 0x1000);

    return MUNIT_OK;
}

static MunitResult
test_unmap_page_complete_pml4_2M(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt.entries[0], !=, 0x1000);

    vmm_map_page_in(&complete_pml4, 0x200000, 0x1000, 0);

    uint64_t *pdpt =
            (uint64_t *)(complete_pml4.entries[0] & 0xFFFFFFFFFFFFF000);
    uint64_t *pd = (uint64_t *)(pdpt[0] & 0xFFFFFFFFFFFFF000);
    uint64_t *pt = (uint64_t *)(pd[1] & 0xFFFFFFFFFFFFF000);

    // Correct page was mapped
    munit_assert_uint64(pt[0], ==, 0x1000);

    uintptr_t unmapped_phys =
            vmm_unmap_page_in((uint64_t *)&complete_pml4, 0x200000);

    // Higher-level tables are untouched
    munit_assert_uint64(complete_pml4.entries[0], ==, (uint64_t)pdpt | PRESENT);
    munit_assert_uint64(pdpt[0], ==, (uint64_t)pd | PRESENT);
    munit_assert_uint64(pd[1], ==, (uint64_t)pt | PRESENT);

    // Correct page was unmapped in PT
    munit_assert_uint64(pt[0], ==, 0);

    // Physical address of previously-mapped page was returned
    munit_assert_uint64(unmapped_phys, ==, 0x1000);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    posix_memalign((void **)&empty_pml4, 0x1000, 0x1000);
    memset(&empty_pml4, 0, 0x1000);

    complete_pml4.entries[0] = ((uint64_t)&complete_pdpt) | PRESENT;
    complete_pdpt.entries[0] = ((uint64_t)&complete_pd) | PRESENT;
    complete_pd.entries[0] = ((uint64_t)&complete_pt) | PRESENT;
    complete_pt.entries[0] = 0;

    return NULL;
}

static void teardown(void *param) { mock_pmm_reset(); }

static MunitTest test_suite_tests[] = {
        /* TODO fix these tests...
        {(char *)"/map/empty_pml4_0M", test_map_page_empty_pml4_0, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/empty_pml4_2M", test_map_page_empty_pml4_2M, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/empty_pml4_1G", test_map_page_empty_pml4_1G, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/empty_pml4_512G", test_map_page_empty_pml4_512G, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
    */
        {(char *)"/map/complete_pml4_0M", test_map_page_complete_pml4_0, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/complete_pml4_phys_4G",
         test_map_page_complete_pml4_phys_4G, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/containing_already", test_map_page_containing_already,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/map/containing_within", test_map_page_containing_within,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/unmap/empty_pml4_0M", test_unmap_page_empty_pml4_0, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/unmap/empty_pml4_2M", test_unmap_page_empty_pml4_2M, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/unmap/complete_pml4_0M", test_unmap_page_complete_pml4_0,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},

        /* TODO fix this test
        {(char *)"/unmap/complete_pml4_2M", test_unmap_page_complete_pml4_2M,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
    */

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/vmm", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
