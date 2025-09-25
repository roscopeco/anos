/*
 * Tests for the virtual memory allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "munit.h"
#include "vmm/vmalloc.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    void *metadata_region;
    size_t metadata_size;
    uint64_t managed_start;
    uint64_t managed_size;
} vmm_fixture_t;

static void *test_setup(const MunitParameter params[], void *user_data) {
    vmm_fixture_t *fixture = malloc(sizeof(vmm_fixture_t));
    munit_assert_not_null(fixture);

    // Allocate metadata region - 4KB should be enough for tests
    fixture->metadata_size = 4096;
    fixture->metadata_region = malloc(fixture->metadata_size);
    munit_assert_not_null(fixture->metadata_region);

    // Set up managed region - 1MB virtual address space
    fixture->managed_start = 0x100000000ULL; // Start at 4GB
    fixture->managed_size = 0x100000;        // 1MB

    // Initialize VMM
    int result =
            vmm_init(fixture->metadata_region, fixture->metadata_size, fixture->managed_start, fixture->managed_size);
    munit_assert_int(result, ==, VMM_SUCCESS);

    return fixture;
}

static void test_teardown(void *fixture) {
    vmm_fixture_t *f = (vmm_fixture_t *)fixture;
    free(f->metadata_region);
    free(f);
}

// Initialisation works correctly
static MunitResult test_init(const MunitParameter params[], void *fixture) {
    vmm_fixture_t *f = (vmm_fixture_t *)fixture;

    // Test invalid parameters
    int result = vmm_init(NULL, f->metadata_size, f->managed_start, f->managed_size);
    munit_assert_int(result, ==, VMM_ERROR_INVALID_PARAMS);

    result = vmm_init(f->metadata_region, 0, f->managed_start, f->managed_size);
    munit_assert_int(result, ==, VMM_ERROR_INVALID_PARAMS);

    result = vmm_init(f->metadata_region, f->metadata_size, f->managed_start, 0);
    munit_assert_int(result, ==, VMM_ERROR_INVALID_PARAMS);

    // Test successful initialization
    result = vmm_init(f->metadata_region, f->metadata_size, f->managed_start, f->managed_size);
    munit_assert_int(result, ==, VMM_SUCCESS);

    return MUNIT_OK;
}

// Allocation of zero pages
static MunitResult test_alloc_zero(const MunitParameter params[], void *fixture) {
    uint64_t addr = vmm_alloc_block(0);
    munit_assert_uint64(addr, ==, 0);
    return MUNIT_OK;
}

// Basic allocation
static MunitResult test_alloc_basic(const MunitParameter params[], void *fixture) {
    vmm_fixture_t *f = (vmm_fixture_t *)fixture;

    // Allocate a single page
    uint64_t addr1 = vmm_alloc_block(1);
    munit_assert_uint64(addr1, !=, 0);
    munit_assert_uint64(addr1, >=, f->managed_start);
    munit_assert_uint64(addr1 + VM_PAGE_SIZE, <=, f->managed_start + f->managed_size);

    // Allocate another page
    uint64_t addr2 = vmm_alloc_block(1);
    munit_assert_uint64(addr2, !=, 0);
    munit_assert_uint64(addr2, !=, addr1);

    return MUNIT_OK;
}

// Allocation until out of space
static MunitResult test_alloc_exhaust(const MunitParameter params[], void *fixture) {
    vmm_fixture_t *f = (vmm_fixture_t *)fixture;

    // Calculate number of pages in managed region
    uint64_t total_pages = f->managed_size / VM_PAGE_SIZE;
    uint64_t last_addr = 0;

    // Allocate all pages one by one
    for (uint64_t i = 0; i < total_pages; i++) {
        uint64_t addr = vmm_alloc_block(1);
        munit_assert_uint64(addr, !=, 0);
        last_addr = addr;
    }

    // Next allocation should fail
    uint64_t addr = vmm_alloc_block(1);
    munit_assert_uint64(addr, ==, 0);

    return MUNIT_OK;
}

// Freeing blocks
static MunitResult test_free_basic(const MunitParameter params[], void *fixture) {
    // Allocate a page
    uint64_t addr = vmm_alloc_block(1);
    munit_assert_uint64(addr, !=, 0);

    // Free it
    int result = vmm_free_block(addr, 1);
    munit_assert_int(result, ==, VMM_SUCCESS);

    // Should be able to allocate it again
    uint64_t new_addr = vmm_alloc_block(1);
    munit_assert_uint64(new_addr, ==, addr);

    return MUNIT_OK;
}

// Freeing with invalid parameters
static MunitResult test_free_invalid(const MunitParameter params[], void *fixture) {
    // Try to free zero pages
    int result = vmm_free_block(0x1000, 0);
    munit_assert_int(result, ==, VMM_ERROR_INVALID_PARAMS);

    // Try to free unaligned address
    result = vmm_free_block(0x1001, 1);
    munit_assert_int(result, ==, VMM_ERROR_INVALID_PARAMS);

    return MUNIT_OK;
}

// Coalescing of free blocks
static MunitResult test_coalesce(const MunitParameter params[], void *fixture) {
    // Allocate three contiguous pages
    uint64_t addr1 = vmm_alloc_block(1);
    uint64_t addr2 = vmm_alloc_block(1);
    uint64_t addr3 = vmm_alloc_block(1);

    munit_assert_uint64(addr1, !=, 0);
    munit_assert_uint64(addr2, ==, addr1 + VM_PAGE_SIZE);
    munit_assert_uint64(addr3, ==, addr2 + VM_PAGE_SIZE);

    // Free them in reverse order
    int result = vmm_free_block(addr3, 1);
    munit_assert_int(result, ==, VMM_SUCCESS);

    result = vmm_free_block(addr2, 1);
    munit_assert_int(result, ==, VMM_SUCCESS);

    result = vmm_free_block(addr1, 1);
    munit_assert_int(result, ==, VMM_SUCCESS);

    // Should be able to allocate all three pages at once
    uint64_t new_addr = vmm_alloc_block(3);
    munit_assert_uint64(new_addr, ==, addr1);

    return MUNIT_OK;
}

// Large allocations
static MunitResult test_large_alloc(const MunitParameter params[], void *fixture) {
    vmm_fixture_t *f = (vmm_fixture_t *)fixture;

    // Try to allocate more pages than available
    uint64_t too_many_pages = (f->managed_size / VM_PAGE_SIZE) + 1;
    uint64_t addr = vmm_alloc_block(too_many_pages);
    munit_assert_uint64(addr, ==, 0);

    // Allocate exactly half the space
    uint64_t half_pages = (f->managed_size / VM_PAGE_SIZE) / 2;
    addr = vmm_alloc_block(half_pages);
    munit_assert_uint64(addr, !=, 0);

    return MUNIT_OK;
}

// Fragmentation and reassembly
static MunitResult test_fragmentation(const MunitParameter params[], void *fixture) {
    // Allocate 10 single pages
    uint64_t addrs[10];
    for (int i = 0; i < 10; i++) {
        addrs[i] = vmm_alloc_block(1);
        munit_assert_uint64(addrs[i], !=, 0);
    }

    // Free every other page
    for (int i = 0; i < 10; i += 2) {
        int result = vmm_free_block(addrs[i], 1);
        munit_assert_int(result, ==, VMM_SUCCESS);
    }

    // Allocate all remaining pages
    uint64_t remaining_addr;
    while ((remaining_addr = vmm_alloc_block(1)) != 0) {
        // Continue allocating
    }

    // Free every other allocated page to create fragmentation
    for (int i = 0; i < 10; i += 2) {
        int result = vmm_free_block(addrs[i], 1);
        munit_assert_int(result, ==, VMM_SUCCESS);
    }

    // Now try to allocate 2 pages - should fail due to fragmentation
    uint64_t addr = vmm_alloc_block(2);
    munit_assert_uint64(addr, ==, 0);

    // Free the remaining pages
    for (int i = 1; i < 10; i += 2) {
        int result = vmm_free_block(addrs[i], 1);
        munit_assert_int(result, ==, VMM_SUCCESS);
    }

    // Now should be able to allocate 10 pages
    addr = vmm_alloc_block(10);
    munit_assert_uint64(addr, ==, addrs[0]);

    return MUNIT_OK;
}

static MunitTest vmm_tests[] = {
        {"/init", test_init, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc/zero", test_alloc_zero, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc/basic", test_alloc_basic, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc/exhaust", test_alloc_exhaust, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc/large_alloc", test_large_alloc, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alloc/fragmentation", test_fragmentation, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/free/basic", test_free_basic, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/free/invalid", test_free_invalid, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/free/coalesce", test_coalesce, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/vmm/alloc/rb", vmm_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) { return munit_suite_main(&test_suite, NULL, argc, argv); }