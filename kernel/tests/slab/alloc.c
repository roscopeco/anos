/*
 * Tests for the slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdlib.h>

#include "munit.h"

#include "fba/alloc.h"
#include "mock_pmm.h"
#include "mock_recursive.h"
#include "mock_vmm.h"
#include "slab/alloc.h"
#include "vmm/vmconfig.h"

#define TEST_PML4_ADDR (((uint64_t *)0x100000))
#define TEST_PAGE_COUNT ((32768))

static const int PAGES_PER_SLAB = BYTES_PER_SLAB / VM_PAGE_SIZE;

static void *test_setup(const MunitParameter params[], void *user_data) {
    void *page_area_ptr;
    posix_memalign(&page_area_ptr, 0x40000, TEST_PAGE_COUNT << 12);
    bool result = fba_init(TEST_PML4_ADDR, (uintptr_t)page_area_ptr, 32768);
    return page_area_ptr;
}

static void test_teardown(void *page_area_ptr) {
    free(page_area_ptr);
    mock_pmm_reset();
    mock_vmm_reset();
}

static inline void *slab_area_base(void *page_area_ptr) {
    // skip one page used by FBA, and three unused by slab alignment
    return (void *)((uint64_t)page_area_ptr + 0x4000);
}

static MunitResult test_slab_init(const MunitParameter params[], void *param) {
    bool result = slab_alloc_init();

    // succeeds
    munit_assert_true(result);

    return MUNIT_OK;
}

static MunitResult
test_slab_alloc_block_from_empty(const MunitParameter params[],
                                 void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *result = slab_alloc_block();

    // Slab allocated a block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Block is at the base of the slab area, plus 64 bytes (Slab* is at the base)
    munit_assert_ptr_equal(result, slab_area_base(page_area_ptr) + 64);

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... List linkage correct
    munit_assert_ptr(slab->this.next, ==, NULL);

    // ... Bitmaps correct - first two blocks allocated, rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_alloc_block_x63(const MunitParameter params[],
                                             void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[63];

    for (int i = 0; i < 63; i++) {
        results[i] = slab_alloc_block();
    }

    // Slab allocated a block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Blocks are at the right location
    for (int i = 0; i < 63; i++) {
        munit_assert_ptr_equal(results[i],
                               slab_area_base(page_area_ptr) + 64 + (i * 64));
    }

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... List linkage correct
    munit_assert_ptr(slab->this.next, ==, NULL);

    // ... Bitmaps correct - first 64 blocks allocated (1 slab header, 63 data), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_alloc_block_x64(const MunitParameter params[],
                                             void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[64];

    for (int i = 0; i < 64; i++) {
        results[i] = slab_alloc_block();
    }

    // Slab allocated a block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Blocks are at the right location
    for (int i = 0; i < 64; i++) {
        munit_assert_ptr_equal(results[i],
                               slab_area_base(page_area_ptr) + 64 + (i * 64));
    }

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... List linkage correct
    munit_assert_ptr(slab->this.next, ==, NULL);

    // ... Bitmaps correct - first 65 blocks allocated (1 slab header, 63 data), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000001);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_alloc_block_x255(const MunitParameter params[],
                                              void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[255];

    for (int i = 0; i < 255; i++) {
        results[i] = slab_alloc_block();
    }

    // Slab allocated a block of PAGES_PER_SLAB size from PMM (via FBA)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Blocks are at the right location
    for (int i = 0; i < 255; i++) {
        munit_assert_ptr_equal(results[i],
                               slab_area_base(page_area_ptr) + 64 + (i * 64));
    }

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... List linkage correct
    munit_assert_ptr(slab->this.next, ==, NULL);

    // ... Bitmaps correct - slab is full
    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap3, ==, 0xffffffffffffffff);

    return MUNIT_OK;
}

static MunitResult test_slab_alloc_block_x256(const MunitParameter params[],
                                              void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[256];

    for (int i = 0; i < 256; i++) {
        results[i] = slab_alloc_block();
    }

    // Slab allocated two block of PAGES_PER_SLAB size from PMM (via FBA)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        (PAGES_PER_SLAB * 2) + 1);

    Slab *first_slab = slab_base(results[0]);
    Slab *second_slab = slab_base(results[255]);

    // Blocks in the first slab are at the right location
    for (int i = 0; i < 255; i++) {
        munit_assert_ptr_equal(results[i], first_slab + 1 + i);
    }

    munit_assert_ptr_equal(results[255], second_slab + 1);

    // Slabs have been initialized correctly...
    // ... first slab...
    // ... List linkage correct

    //      should be no next, this slab is now full and at end of list...
    munit_assert_ptr(first_slab->this.next, ==, NULL);

    // ... Bitmaps correct - slab is full
    munit_assert_uint64(first_slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap3, ==, 0xffffffffffffffff);

    // ... second slab...
    // ... List linkage correct

    //      should be no next, this slab is partial...
    munit_assert_ptr(second_slab->this.next, ==, NULL);

    // ... Bitmaps correct - first 2 blocks allocated (1 slab header, 1 data), rest are free
    munit_assert_uint64(second_slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(second_slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(second_slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(second_slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_alloc_block_x512(const MunitParameter params[],
                                              void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[512];

    for (int i = 0; i < 512; i++) {
        results[i] = slab_alloc_block();
    }

    // Slab allocated three block of PAGES_PER_SLAB size from PMM (via FBA)
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        (PAGES_PER_SLAB * 3) + 1);

    Slab *first_slab = slab_base(results[0]);
    Slab *second_slab = slab_base(results[255]);
    Slab *third_slab = slab_base(results[511]);

    // Blocks in the first slab are at the right location
    for (int i = 0; i < 255; i++) {
        munit_assert_ptr_equal(results[i], first_slab + 1 + i);
    }

    // Blocks in the second slab are at the right location
    for (int i = 255; i < 510; i++) {
        munit_assert_ptr_equal(results[i], second_slab + 1 + (i - 255));
    }

    // And the last two blocks are in the third slab (because we lost
    // one each in the first two to slab metadata blocks).
    munit_assert_ptr_equal(results[510], third_slab + 1);
    munit_assert_ptr_equal(results[511], third_slab + 2);

    // Slabs have been initialized correctly...
    // ... first slab...
    // ... List linkage correct

    //      We have two slabs full, they should be in the same list,
    //      but this should be at the end so no next!
    munit_assert_ptr(first_slab->this.next, ==, NULL);

    // ... Bitmaps correct - slab is full
    munit_assert_uint64(first_slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(first_slab->bitmap3, ==, 0xffffffffffffffff);

    // ... second slab...
    // ... List linkage correct
    //      first_slab should be next in the full list...
    munit_assert_ptr(second_slab->this.next, ==, first_slab);

    // ... Bitmaps correct - slab is full
    munit_assert_uint64(second_slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(second_slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(second_slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(second_slab->bitmap3, ==, 0xffffffffffffffff);

    // ... third slab...
    // ... List linkage correct
    munit_assert_ptr(third_slab->this.next, ==,
                     NULL); // should be no next, this slab is partial...

    // ... Bitmaps correct - first 3 blocks allocated (1 slab header, 2 data), rest are free
    munit_assert_uint64(third_slab->bitmap0, ==, 0x0000000000000007);
    munit_assert_uint64(third_slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(third_slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(third_slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_free_not_allocated(const MunitParameter params[],
                                                void *page_area_ptr) {
    void *test_block = slab_alloc_block();

    // Slab allocated an FBA block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... Bitmaps correct - first block allocated (to bitmap), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    slab_free(slab + 2);

    // ... Bitmaps remain correct
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_free_bitmap(const MunitParameter params[],
                                         void *page_area_ptr) {
    void *test_block = slab_alloc_block();

    // Slab allocated an FBA block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... Bitmaps correct - first two blocks allocated (to bitmap and test block), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    slab_free(slab);

    // ... Nothing happened, bitmaps remain correct
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_free_one(const MunitParameter params[],
                                      void *page_area_ptr) {
    void *test_block = slab_alloc_block();

    // Slab allocated an FBA block
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... Bitmaps correct - first two blocks allocated (to bitmap and test block), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000003);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    slab_free(test_block);

    // ... Block freed, bitmaps remain correct
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000001);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_free_two(const MunitParameter params[],
                                      void *page_area_ptr) {
    void *test_block_1 = slab_alloc_block();
    void *test_block_2 = slab_alloc_block();

    // Slab allocated two FBA blocks
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==,
                        PAGES_PER_SLAB + 1);

    // Slab has been initialized correctly...
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    // ... Bitmaps correct - first three blocks allocated (to bitmap and test blocks), rest are free
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000007);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    slab_free(test_block_1);

    // ... First block freed, bitmaps remain correct
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000005);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    slab_free(test_block_2);

    // ... First block freed, bitmaps remain correct
    munit_assert_uint64(slab->bitmap0, ==, 0x0000000000000001);
    munit_assert_uint64(slab->bitmap1, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap2, ==, 0x0000000000000000);
    munit_assert_uint64(slab->bitmap3, ==, 0x0000000000000000);

    return MUNIT_OK;
}

static MunitResult test_slab_free_from_full(const MunitParameter params[],
                                            void *page_area_ptr) {
    // one block allocated for FBA already
    munit_assert_uint32(mock_pmm_get_total_page_allocs(), ==, 1);

    void *results[255];

    for (int i = 0; i < 255; i++) {
        results[i] = slab_alloc_block();
    }

    // ... Bitmaps correct - slab is full
    Slab *slab = (Slab *)slab_area_base(page_area_ptr);

    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap3, ==, 0xffffffffffffffff);

    // when block is freed within slab...
    slab_free(results[127]);

    // ... Then bitmaps remain correct - slab no longer full
    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap2, ==, 0xfffffffffffffffe);
    munit_assert_uint64(slab->bitmap3, ==, 0xffffffffffffffff);

    // if we then reallocate a block...
    void *realloc_block = slab_alloc_block();

    // then it goes back into that slab, which is full again
    munit_assert_uint64(slab->bitmap0, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap1, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap2, ==, 0xffffffffffffffff);
    munit_assert_uint64(slab->bitmap3, ==, 0xffffffffffffffff);

    // so the block has the same address...
    munit_assert_ptr(realloc_block, ==, results[127]);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/init", test_slab_init, NULL, NULL, MUNIT_TEST_OPTION_NONE,
         NULL},

        {(char *)"/alloc/block_from_empty", test_slab_alloc_block_from_empty,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_x63", test_slab_alloc_block_x63, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_x64", test_slab_alloc_block_x64, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_x255", test_slab_alloc_block_x255, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_x256", test_slab_alloc_block_x256, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/block_x512", test_slab_alloc_block_x512, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc/free_not_alloc", test_slab_free_not_allocated,
         test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/free_bitmap_nope", test_slab_free_bitmap, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/free_one", test_slab_free_one, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/free_two", test_slab_free_two, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc/free_two", test_slab_free_from_full, test_setup,
         test_teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/slab", test_suite_tests, NULL,
                                      1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
