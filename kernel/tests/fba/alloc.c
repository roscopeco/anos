/*
 * Tests for the fixed-block allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * These tests are unfortunately quite brittle, since they
 * do a lot of implementation testing...
 */

#include <stdint.h>
#include <stdlib.h>

#include "fba/alloc.h"
#include "mock_pmm.h"
#include "mock_vmm.h"
#include "munit.h"
#include "structs/bitmap.h"
#include "vmm/vmmapper.h"

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((4)) // only need allocated mem for the bitmap...

// These are conditionally defined in the main code, which sucks, but works "for
// now"...
uintptr_t test_fba_check_begin();
uintptr_t test_fba_check_size();
uint64_t *test_fba_bitmap();
uint64_t *test_fba_bitmap_end();

static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
    mock_vmm_reset();
}

static MunitResult test_fba_init_zero(const MunitParameter params[],
                                      void *param) {
    bool result = fba_init(0, 0, 0);

    // succeeds
    munit_assert_true(result);

    // State is set correctly
    munit_assert_uint64((uint64_t)test_fba_check_begin(), ==, 0);
    munit_assert_uint64((uint64_t)test_fba_check_size(), ==, 0);

    // No pages allocated for the bitmap because zero size...
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_fba_init_unaligned_begin(const MunitParameter params[],
                                                 void *param) {
    bool result;

    result = fba_init(TEST_PML4_ADDR, 0x001, 100);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0xfff, 100);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0x1001, 100);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0x1fff, 100);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult
test_fba_init_size_not_multiple(const MunitParameter params[], void *param) {
    bool result;

    result = fba_init(TEST_PML4_ADDR, 0x1000, 1);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0x1000, 32767);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0x1000, 32769);
    munit_assert_false(result);

    result = fba_init(TEST_PML4_ADDR, 0x1000, 65535);
    munit_assert_false(result);

    return MUNIT_OK;
}

static MunitResult test_fba_init_32768_ok(const MunitParameter params[],
                                          void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    // succeeds
    munit_assert_true(result);

    // State is set correctly
    munit_assert_ptr((void *)test_fba_check_begin(), ==, test_page_area);
    munit_assert_uint64((uint64_t)test_fba_check_size(), ==, 32768);

    // One page allocated for bitmap (32768 bits)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 1);

    // Page was mapped into the correct place (first page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        (uint64_t)test_page_area);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    // Bitmap and bitmap end are set correctly
    munit_assert_ptr_equal(test_fba_bitmap(), (uint64_t *)test_page_area);
    munit_assert_ptr_equal(test_fba_bitmap_end(),
                           (uint64_t *)test_page_area +
                                   0x200 /* 512 longs in a page... */);

    // Page contains expected bitmap, with first block allocated for bitmap
    // itself
    munit_assert_uint64(*((uint64_t *)test_page_area), ==, 0x0000000000000001);

    return MUNIT_OK;
}

static MunitResult test_fba_init_65536_ok(const MunitParameter params[],
                                          void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 65536);

    // succeeds
    munit_assert_true(result);

    // State is set correctly
    munit_assert_ptr((void *)test_fba_check_begin(), ==, test_page_area);
    munit_assert_uint64((uint64_t)test_fba_check_size(), ==, 65536);

    // Two pages allocated for bitmap (65536 bits)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (second page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x1000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    // Bitmap and bitmap end are set correctly
    munit_assert_ptr_equal(test_fba_bitmap(), (uint64_t *)test_page_area);
    munit_assert_ptr_equal(test_fba_bitmap_end(),
                           (uint64_t *)test_page_area +
                                   0x400 /* 1024 longs in 2 pages... */);

    // Page contains expected bitmap, with first two blocks allocated for bitmap
    // itself
    munit_assert_uint64(*((uint64_t *)test_page_area), ==, 0x0000000000000003);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_block_nospace_zero(const MunitParameter params[],
                                  void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 0);

    munit_assert_true(result);

    munit_assert_ptr_null(fba_alloc_block());

    return MUNIT_OK;
}

static MunitResult test_fba_alloc_block_one(const MunitParameter params[],
                                            void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    munit_assert_ptr_equal(fba_alloc_block(),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // Two pages allocated (one for bitmap, one for the block itself)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (second page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x1000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult test_fba_alloc_block_two(const MunitParameter params[],
                                            void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // Can allocate two pages sequentially
    munit_assert_ptr_equal(fba_alloc_block(),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));
    munit_assert_ptr_equal(fba_alloc_block(),
                           (uint64_t *)((uint64_t)test_page_area + 0x2000));

    // Three pages allocated (one for bitmap, two for the blocks themselves)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 3);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 3);

    // Last page was mapped into the correct place (third page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x2000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x2000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_block_exhaustion(const MunitParameter params[],
                                void *test_page_area) {

    // Given we have 32768 total blocks (of which 1 will be used for the bitmap)
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    // We can allocate 32767 blocks...
    for (int i = 0; i < 32767; i++) {
        munit_assert_ptr_equal(
                fba_alloc_block(),
                (uint64_t *)((uint64_t)test_page_area +
                             /*bitmap page = */ 0x1000 +
                             (/*already allocated pages =*/i * 0x1000)));
    }

    // but 32768 is a bridge too far...
    munit_assert_ptr_null(fba_alloc_block());

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_nospace_zero(const MunitParameter params[],
                                   void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 0);

    munit_assert_true(result);

    munit_assert_ptr_null(fba_alloc_blocks(1));

    return MUNIT_OK;
}

static MunitResult test_fba_alloc_blocks_one(const MunitParameter params[],
                                             void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    munit_assert_ptr_equal(fba_alloc_blocks(1),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // Two pages allocated (one for bitmap, one for the block itself)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (second page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x1000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult test_fba_alloc_blocks_two(const MunitParameter params[],
                                             void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // Can allocate two pages sequentially
    munit_assert_ptr_equal(fba_alloc_blocks(2),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // Three pages allocated (one for bitmap, two for the blocks themselves)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 3);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 3);

    // Last page was mapped into the correct place (third page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x2000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x2000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult test_fba_alloc_blocks_max(const MunitParameter params[],
                                             void *test_page_area) {

    // Given we have 32768 total blocks (of which 1 will be used for the bitmap)
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    // Can allocate 32768 pages sequentially
    munit_assert_ptr_equal(fba_alloc_blocks(32767),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // 32768 pages allocated (1 for bitmap, 32767 for the blocks themselves)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 32768);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 32768);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_exhaustion(const MunitParameter params[],
                                 void *test_page_area) {

    // Given we have 32768 total blocks (of which 1 will be used for the bitmap)
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    munit_assert_ptr_null(fba_alloc_blocks(32768));

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_1(const MunitParameter params[],
                                void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 1),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // Two pages allocated (one for bitmap, one for the block itself)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (second page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x1000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_2(const MunitParameter params[],
                                void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // Already 1 block allocated (for bitmap) so this should skip the next block.
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 2),
                           (uint64_t *)((uint64_t)test_page_area + 0x2000));

    // Two pages allocated (one for bitmap, one for the block itself)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (third page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x2000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_4(const MunitParameter params[],
                                void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // Already 1 block allocated (for bitmap) so this should skip the next three blocks.
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 4),
                           (uint64_t *)((uint64_t)test_page_area + 0x4000));

    // Two pages allocated (one for bitmap, one for the block itself)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 2);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 2);

    // Last page was mapped into the correct place (fifth page in the area)...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==,
                        TEST_PMM_NOALLOC_START_ADDRESS + 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ((uint64_t)test_page_area) + 0x4000);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==,
                        PG_WRITE | PG_PRESENT);
    munit_assert_uint64(mock_vmm_get_last_page_map_pml4(), ==,
                        (uint64_t)TEST_PML4_ADDR);

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_leaves_free(const MunitParameter params[],
                                          void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // Already 1 block allocated (for bitmap) so this should skip the next three blocks.
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 4),
                           (uint64_t *)((uint64_t)test_page_area + 0x4000));

    // Subsequent alloc align 2 still uses free space below previous align
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 2),
                           (uint64_t *)((uint64_t)test_page_area + 0x2000));

    // Subsequent alloc align 1 still uses free space below previous align
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 1),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_already(const MunitParameter params[],
                                      void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // 1 block aligned for bitmap, alloc another one to make two...
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 1),
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    // alloc aligned 2 should take next slot as we're already aligned...
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 2),
                           (uint64_t *)((uint64_t)test_page_area + 0x2000));

    // Subsequent alloc align 4 now uses next slot, since we're already aligned
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 4),
                           (uint64_t *)((uint64_t)test_page_area + 0x4000));

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_max(const MunitParameter params[],
                                  void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // 1 block aligned for bitmap, so this needs to push out by another 63 blocks...
    munit_assert_ptr_equal(fba_alloc_blocks_aligned(1, 64),
                           (uint64_t *)((uint64_t)test_page_area + 0x40000));

    return MUNIT_OK;
}

static MunitResult
test_fba_alloc_blocks_aligned_invalid(const MunitParameter params[],
                                      void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);

    munit_assert_true(result);

    // 1 block aligned for bitmap, alloc another one to make two...
    munit_assert_ptr_null(fba_alloc_blocks_aligned(1, 0));

    // alloc aligned 2 should take next slot as we're already aligned...
    munit_assert_ptr_null(fba_alloc_blocks_aligned(1, 128));

    return MUNIT_OK;
}

static MunitResult test_fba_free_single_block(const MunitParameter params[],
                                              void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    void *alloc = fba_alloc_block();
    munit_assert_ptr_equal(alloc,
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));

    fba_free(alloc);

    // Verify that the block is marked as free
    munit_assert_false(bitmap_check(test_fba_bitmap(), 1));

    // Verify that the page is unmapped
    munit_assert_uint32(mock_vmm_get_total_page_unmaps(), ==, 1);

    // Verify that the physical page was freed
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, 1);

    return MUNIT_OK;
}

static MunitResult test_fba_free_multiple_blocks(const MunitParameter params[],
                                                 void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    void *alloc1 = fba_alloc_block();
    void *alloc2 = fba_alloc_block();
    munit_assert_ptr_equal(alloc1,
                           (uint64_t *)((uint64_t)test_page_area + 0x1000));
    munit_assert_ptr_equal(alloc2,
                           (uint64_t *)((uint64_t)test_page_area + 0x2000));

    fba_free(alloc1);
    fba_free(alloc2);

    // Verify that the blocks are marked as free
    munit_assert_false(bitmap_check(test_fba_bitmap(), 1));
    munit_assert_false(bitmap_check(test_fba_bitmap(), 2));

    // Verify that the pages are unmapped
    munit_assert_uint32(mock_vmm_get_total_page_unmaps(), ==, 2);

    // Verify that the physical pages were freed
    munit_assert_uint32(mock_pmm_get_total_page_frees(), ==, 2);

    return MUNIT_OK;
}

static MunitResult
test_fba_free_unallocated_block(const MunitParameter params[],
                                void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    // Attempt to free a block that was not allocated
    fba_free((void *)((uint64_t)test_page_area + 0x1000));

    // Verify that the state remains unchanged
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 1);
    munit_assert_uint32(mock_vmm_get_total_page_unmaps(), ==, 0);

    return MUNIT_OK;
}

static MunitResult test_fba_free_invalid_address(const MunitParameter params[],
                                                 void *test_page_area) {
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)test_page_area, 32768);
    munit_assert_true(result);

    // Attempt to free a block with an address outside the allocated range
    fba_free((void *)((uint64_t)test_page_area + 0x10000));

    // Verify that the state remains unchanged
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);
    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 1);
    munit_assert_uint32(mock_vmm_get_total_page_unmaps(), ==, 0);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init/zero", test_fba_init_zero, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init/unaligned_begin", test_fba_init_unaligned_begin, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init/size_not_multiple", test_fba_init_size_not_multiple,
         NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init/32768_ok", test_fba_init_32768_ok, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init/65536_ok", test_fba_init_65536_ok, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc/block_nospace_zero", test_fba_alloc_block_nospace_zero,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_one", test_fba_alloc_block_one, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_two", test_fba_alloc_block_two, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_exhaustion", test_fba_alloc_block_exhaustion,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc/blocks_nospace_zero",
         test_fba_alloc_blocks_nospace_zero, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_one", test_fba_alloc_blocks_one, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_two", test_fba_alloc_blocks_two, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_max", test_fba_alloc_blocks_max, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_exhaustion", test_fba_alloc_blocks_exhaustion,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc/blocks_aligned_1", test_fba_alloc_blocks_aligned_1,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_2", test_fba_alloc_blocks_aligned_2,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_4", test_fba_alloc_blocks_aligned_4,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_leaves_free",
         test_fba_alloc_blocks_aligned_leaves_free, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_already",
         test_fba_alloc_blocks_aligned_already, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_max", test_fba_alloc_blocks_aligned_max,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/blocks_aligned_invalid",
         test_fba_alloc_blocks_aligned_invalid, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/free/single_block", test_fba_free_single_block, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free/multiple_blocks", test_fba_free_multiple_blocks,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free/unallocated_block", test_fba_free_unallocated_block,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free/invalid_address", test_fba_free_invalid_address,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/fba", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
