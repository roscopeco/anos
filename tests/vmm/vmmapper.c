/*
 * Tests for the virtual memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdio.h>
#include <stdlib.h>

#include "munit.h"
#include "vmm/vmmapper.h"

static uint64_t *empty_pml4;

static uint64_t *complete_pml4;
static uint64_t *complete_pdpt;
static uint64_t *complete_pd;
static uint64_t *complete_pt;

static uint8_t total_page_allocs = 0;

static MunitResult test_map_page_empty_pml4_0(const MunitParameter params[],
                                              void *param) {
    vmm_map_page(empty_pml4, 0x0, 0x1000, 0);

    munit_assert_uint64(empty_pml4[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4[0] & 0xFFFFFFFFFFFFF000);
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
    munit_assert_uint8(total_page_allocs, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_2M(const MunitParameter params[],
                                               void *param) {
    vmm_map_page(empty_pml4, 0x200000, 0x1000, 0);

    munit_assert_uint64(empty_pml4[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4[0] & 0xFFFFFFFFFFFFF000);
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
    munit_assert_uint8(total_page_allocs, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_1G(const MunitParameter params[],
                                               void *param) {
    vmm_map_page(empty_pml4, 0x40000000, 0x1000, 0);

    munit_assert_uint64(empty_pml4[0], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4[0] & 0xFFFFFFFFFFFFF000);

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
    munit_assert_uint8(total_page_allocs, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_empty_pml4_512G(const MunitParameter params[],
                                                 void *param) {
    vmm_map_page(empty_pml4, 0x8000000000, 0x1000, 0);

    munit_assert_uint64(empty_pml4[0], ==, 0);
    munit_assert_uint64(empty_pml4[1], !=, 0);

    // pdpt was created and mapped
    uint64_t *pdpt = (uint64_t *)(empty_pml4[1] & 0xFFFFFFFFFFFFF000);

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
    munit_assert_uint8(total_page_allocs, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_map_page_complete_pml4_0(const MunitParameter params[],
                                                 void *param) {
    munit_assert_uint64(complete_pt[0], !=, 0x1000);

    vmm_map_page(complete_pml4, 0x0, 0x1000, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt[0], ==, 0x1000);

    // No pages were allocated
    munit_assert_uint8(total_page_allocs, ==, 0);

    return MUNIT_OK;
}

static MunitResult
test_map_page_containing_already(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt[0], !=, 0x1000);

    vmm_map_page_containing(complete_pml4, 0x0, 0x1000, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt[0], ==, 0x1000);

    return MUNIT_OK;
}

static MunitResult
test_map_page_containing_within(const MunitParameter params[], void *param) {
    munit_assert_uint64(complete_pt[0], !=, 0x1000);

    vmm_map_page_containing(complete_pml4, 0x0, 0x1234, 0);

    // Correct page was mapped
    munit_assert_uint64(complete_pt[0], ==, 0x1000);

    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    posix_memalign((void **)&empty_pml4, 0x1000, 0x1000);
    memset(empty_pml4, 0, 0x1000);

    posix_memalign((void **)&complete_pml4, 0x1000, 0x1000);
    memset(complete_pml4, 0, 0x1000);
    posix_memalign((void **)&complete_pdpt, 0x1000, 0x1000);
    memset(complete_pdpt, 0, 0x1000);
    posix_memalign((void **)&complete_pd, 0x1000, 0x1000);
    memset(complete_pd, 0, 0x1000);
    posix_memalign((void **)&complete_pt, 0x1000, 0x1000);
    memset(complete_pt, 0, 0x1000);

    complete_pml4[0] = ((uint64_t)complete_pdpt) | PRESENT;
    complete_pdpt[0] = ((uint64_t)complete_pd) | PRESENT;
    complete_pd[0] = ((uint64_t)complete_pt) | PRESENT;
    complete_pt[0] = 0;

    return NULL;
}

void free_alloc_pages();

static void teardown(void *param) {
    free_alloc_pages();
    free(empty_pml4);
    free(complete_pml4);
    free(complete_pdpt);
    free(complete_pd);
    free(complete_pt);
}

static MunitTest test_suite_tests[] = {
        {(char *)"/vmm/map/empty_pml4_0M", test_map_page_empty_pml4_0, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/empty_pml4_2M", test_map_page_empty_pml4_2M, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/empty_pml4_1G", test_map_page_empty_pml4_1G, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/empty_pml4_512G", test_map_page_empty_pml4_512G,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/complete_pml4_0M", test_map_page_complete_pml4_0,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/containing_already",
         test_map_page_containing_already, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/vmm/map/containing_within", test_map_page_containing_within,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}

/* ********** Mocks ************ */

#include "pmm/pagealloc.h"

#define MAX_PAGES 128

MemoryRegion *physical_region;
uint64_t *pages[MAX_PAGES];
uint8_t page_ptr = 0;

uint64_t page_alloc(MemoryRegion *region) {
    if (page_ptr == (MAX_PAGES - 1)) {
        fprintf(stderr, "\n\nWARN: Mock page allocator is out of space 😱\n\n");
        return 0;
    }

    total_page_allocs++;
    posix_memalign((void **)&pages[page_ptr], 0x1000, 0x1000);
    return (uint64_t)pages[page_ptr++];
}

void free_alloc_pages() {
    while (page_ptr > 0) {
        free(pages[--page_ptr]);
    }
    total_page_allocs = 0;
}
