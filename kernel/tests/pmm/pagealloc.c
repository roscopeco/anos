/*
 * Tests for the page allocator
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "pmm/pagealloc.h"
#include "munit.h"

static void *region_buffer;

static E820h_MemMap *create_mem_map(int num_entries) {
    E820h_MemMap *map = munit_malloc(sizeof(E820h_MemMap) +
                                     sizeof(E820h_MemMapEntry) * num_entries);
    map->num_entries = num_entries;
    return map;
}

static MemoryBlock *stack_base(MemoryRegion *region) {
    return ((MemoryBlock *)(region + 1)) - 1;
}

static MunitResult test_init_empty(const MunitParameter params[], void *param) {
    E820h_MemMap map = {.num_entries = 0};

    MemoryRegion *region = page_alloc_init_e820(&map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    return MUNIT_OK;
}

static MunitResult test_init_all_invalid(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_INVALID};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_reserved(const MunitParameter params[],
                                          void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_RESERVED};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi(const MunitParameter params[],
                                      void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_ACPI};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi_nvs(const MunitParameter params[],
                                          void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_ACPI_NVS};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_unusable(const MunitParameter params[],
                                          void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_UNUSABLE};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_disabled(const MunitParameter params[],
                                          void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_DISABLED};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_persistent(const MunitParameter params[],
                                            void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_PERSISTENT};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_unknown(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_UNKNOWN};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_illegal(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry = {.type = 99};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_zero_length(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_AVAILABLE,
                               .base = 0x0000000000000000,
                               .length = 0x0000000000000000,
                               .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_too_small(const MunitParameter params[],
                                       void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_AVAILABLE,
                               .base = 0x0000000000000000,
                               .length = 0x0000000000000400,
                               .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_one_available(const MunitParameter params[],
                                           void *param) {
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_AVAILABLE,
                               .base = 0x0000000000000000,
                               .length = 0x0000000000100000,
                               .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_unaligned_zero(const MunitParameter params[],
                                            void *param) {
    // One block, unaligned. When aligned, it will give us zero bytes
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_AVAILABLE,
                               .base = 0x0000000000000400,
                               .length = 0x0000000000001080,
                               .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0x0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_unaligned_one(const MunitParameter params[],
                                           void *param) {
    // One block, unaligned. When aligned, it will give us 4KiB
    E820h_MemMapEntry entry = {.type = MEM_MAP_ENTRY_AVAILABLE,
                               .base = 0x0000000000000400,
                               .length = 0x0000000000002080,
                               .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0x1000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0x1000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_some_available(const MunitParameter params[],
                                            void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0010000000000000,
                                .length = 0x0100000000100000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000100000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_1M_at_zero(const MunitParameter params[],
                                        void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0010000000000000,
                                .length = 0x0100000000100000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000100000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x100000);
    munit_assert_uint64(region->free, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_large_region(const MunitParameter params[],
                                          void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x10000000);
    munit_assert_uint64(region->free, ==, 0x10000000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x10000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_regions(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000100000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0010000000000000,
                                .length = 0x0100000000100000,
                                .attrs = 0};
    E820h_MemMapEntry entry2 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000100000,
                                .length = 0x0000000000020000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x00120000);
    munit_assert_uint64(region->free, ==, 0x00120000);

    // Two entries on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 2);

    // Top entry is based at 1MiB, 32 pages
    munit_assert_uint64(region->sp->base, ==, 0x100000);
    munit_assert_uint64(region->sp->size, ==, 0x20);

    // Next entry is based at 0MiB, 256 pages
    munit_assert_uint64((region->sp - 1)->base, ==, 0x0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_large_regions(const MunitParameter params[],
                                               void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000010000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x20000000);
    munit_assert_uint64(region->free, ==, 0x20000000);

    // Two entries on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 2);

    // Top entry is based at 256MiB, 65536 pages
    munit_assert_uint64(region->sp->base, ==, 0x10000000);
    munit_assert_uint64(region->sp->size, ==, 0x10000);

    // Next entry is based at 0MiB, 65536 pages
    munit_assert_uint64((region->sp - 1)->base, ==, 0x0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x10000);

    free(map);
    return MUNIT_OK;
}

static MunitResult
test_init_two_noncontig_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000040000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x20000000);
    munit_assert_uint64(region->free, ==, 0x20000000);

    // Two entries on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 2);

    // Top entry is based at 1GiB, 65536 pages
    munit_assert_uint64(region->sp->base, ==, 0x40000000);
    munit_assert_uint64(region->sp->size, ==, 0x10000);

    // Next entry is based at 0MiB, 65536 pages
    munit_assert_uint64((region->sp - 1)->base, ==, 0x0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x10000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_unequal_regions(const MunitParameter params[],
                                                 void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000010000000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000010000000,
                                .length = 0x0000000000100000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    munit_assert_uint64(region->size, ==, 0x10100000);
    munit_assert_uint64(region->free, ==, 0x10100000);

    // Two entries on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 2);

    // Top entry is based at 1GiB, 256 pages
    munit_assert_uint64(region->sp->base, ==, 0x10000000);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    // Next entry is based at 0MiB, 65536 pages
    munit_assert_uint64((region->sp - 1)->base, ==, 0x0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x10000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_empty(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000000100,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    uint64_t page = page_alloc(region);

    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc(region);
    munit_assert_uint64(page, ==, 0);

    // No more pages
    page = page_alloc(region);
    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_two_pages(const MunitParameter params[],
                                        void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Two pages total, two free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x2000);

    // Top entry is based at 0, 2 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x2);

    // Can allocate the first available page
    uint64_t page = page_alloc(region);
    munit_assert_uint64(page, ==, 0);

    // Two pages total, only one free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x1000);

    // Top entry is based at 0x1000, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x1000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Can allocate the last available page
    page = page_alloc(region);
    munit_assert_uint64(page, ==, 0x1000);

    // Two pages total, none free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_two_blocks(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0010000000000000,
                                .length = 0x0100000000100000,
                                .attrs = 0};
    E820h_MemMapEntry entry2 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000100000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Two pages total, two free
    munit_assert_uint64(region->size, ==, 0x3000);
    munit_assert_uint64(region->free, ==, 0x3000);

    // Top entry is based at 1MiB, 2 pages
    munit_assert_uint64(region->sp->base, ==, 0x100000);
    munit_assert_uint64(region->sp->size, ==, 0x2);

    // Can allocate the first available page
    uint64_t page = page_alloc(region);
    munit_assert_uint64(page, ==, 0x100000);

    // Two pages total, only one free
    munit_assert_uint64(region->size, ==, 0x3000);
    munit_assert_uint64(region->free, ==, 0x2000);

    // Top entry is based at 1MiB + 4KiB, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x101000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Can allocate the second available page (last one in the first block)
    page = page_alloc(region);
    munit_assert_uint64(page, ==, 0x101000);

    // Two pages total, none free
    munit_assert_uint64(region->size, ==, 0x3000);
    munit_assert_uint64(region->free, ==, 0x1000);

    // Top entry is based at 0, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Can allocate the third available page (only one in the last block)
    page = page_alloc(region);
    munit_assert_uint64(page, ==, 0);

    // Two pages total, none free
    munit_assert_uint64(region->size, ==, 0x3000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_empty_one(const MunitParameter params[],
                                               void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000000100,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    uint64_t page = page_alloc_m(region, 1);

    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_one(const MunitParameter params[],
                                         void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc_m(region, 1);
    munit_assert_uint64(page, ==, 0);

    // No more pages
    page = page_alloc_m(region, 1);
    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_empty_two(const MunitParameter params[],
                                               void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_RESERVED,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000000100,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    uint64_t page = page_alloc_m(region, 2);

    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_one_from_one(const MunitParameter params[],
                                                  void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc_m(region, 1);
    munit_assert_uint64(page, ==, 0);

    // No more pages
    page = page_alloc(region);
    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_two_from_one(const MunitParameter params[],
                                                  void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can't allocate two pages from the one available page
    uint64_t page = page_alloc_m(region, 2);
    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_two_from_two(const MunitParameter params[],
                                                  void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the two available pages
    uint64_t page = page_alloc_m(region, 2);
    munit_assert_uint64(page, ==, 0);

    // No more pages
    page = page_alloc(region);
    munit_assert_uint64(page & 0xFF, ==, 0xFF);

    free(map);
    return MUNIT_OK;
}

static MunitResult
test_alloc_page_m_not_top_split(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000010000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000008000,
                                .length = 0x0000000000003000,
                                .attrs = 0};
    E820h_MemMapEntry entry2 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000100000,
                                .length = 0x0000000000001000,
                                .attrs = 0};

    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc_m(region, 2);
    munit_assert_uint64(page, ==, 0x8000);

    // Five pages total, three free
    munit_assert_uint64(region->size, ==, 0x5000);
    munit_assert_uint64(region->free, ==, 0x3000);

    // Top entry is based at 1MiB, still 1 page
    munit_assert_uint64(region->sp->base, ==, 0x100000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Alloc came from second entry, and split the remainder so one left
    munit_assert_uint64((region->sp - 1)->base, ==, 0x9000);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x1);

    // Third entry is still at 0, still one page
    munit_assert_uint64((region->sp - 2)->base, ==, 0x10000);
    munit_assert_uint64((region->sp - 2)->size, ==, 0x1);

    free(map);
    return MUNIT_OK;
}

static MunitResult
test_alloc_page_m_not_top_remove(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000008000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMapEntry entry2 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000100000,
                                .length = 0x0000000000001000,
                                .attrs = 0};

    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc_m(region, 2);
    munit_assert_uint64(page, ==, 0x8000);

    // Four pages total, two free
    munit_assert_uint64(region->size, ==, 0x4000);
    munit_assert_uint64(region->free, ==, 0x2000);

    // Top entry is based at 1MiB, still 1 page
    munit_assert_uint64(region->sp->base, ==, 0x100000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Second entry is now the one at at 0, still one page
    // Original second entry was removed from the stack
    munit_assert_uint64((region->sp - 1)->base, ==, 0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x1);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page_m_top_remove(const MunitParameter params[],
                                                void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMapEntry entry1 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0010000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMapEntry entry2 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000100000,
                                .length = 0x0000000000002000,
                                .attrs = 0};

    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);

    // Can allocate the one available page
    uint64_t page = page_alloc_m(region, 2);
    munit_assert_uint64(page, ==, 0x100000);

    // Four pages total, two free
    munit_assert_uint64(region->size, ==, 0x4000);
    munit_assert_uint64(region->free, ==, 0x2000);

    // Top entry is now based at 0x0010000000000000, still 1 page
    // Original top entry was removed from the stack
    munit_assert_uint64(region->sp->base, ==, 0x0010000000000000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Second entry is now the one at at 0, still one page
    munit_assert_uint64((region->sp - 1)->base, ==, 0);
    munit_assert_uint64((region->sp - 1)->size, ==, 0x1);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_free_page(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    uint64_t page = page_alloc(region);

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    // One page total, none free
    munit_assert_uint64(region->size, ==, 0x1000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Free the page
    page_free(region, page);

    // Stack is no longer empty
    munit_assert_ptr_equal(region->sp, region + 1);

    // Top entry is based at 0, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // One page total, one free
    munit_assert_uint64(region->size, ==, 0x1000);
    munit_assert_uint64(region->free, ==, 0x1000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_free_unaligned_page(const MunitParameter params[],
                                            void *param) {
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000001000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    uint64_t page = page_alloc(region);

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    // One page total, none free
    munit_assert_uint64(region->size, ==, 0x1000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Free an unaligned address
    page_free(region, 0x100F);

    // Stack is still empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_free_contig_pages_forward(const MunitParameter params[],
                                                  void *param) {
    // Special case - if we free contiguous pages, where the page being freed is
    // **above** the one at the stack top, we should coalesce them.
    //
    // Not sure how useful this will end up being, but it's cheap and worth
    // testing, should maybe add some metrics to see how common it is...
    //
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    uint64_t page1 = page_alloc(region); // will be at 0x1000
    uint64_t page2 = page_alloc(region); // will be at 0x2000

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    // Two pages total, none free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Free the lower page
    page_free(region, page1);

    // Stack is no longer empty
    munit_assert_ptr_equal(region->sp, region + 1);

    // Top entry is based at 0, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Two pages total, one free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x1000);

    // Free the higher page
    page_free(region, page2);

    // Top entry still based at 0, but now 2 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x2);

    // Two pages total, two free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x2000);

    free(map);
    return MUNIT_OK;
}

static MunitResult
test_free_contig_pages_backward(const MunitParameter params[], void *param) {
    // Special case - if we free contiguous pages, where the page being freed is
    // **below** the one at the stack top, we should coalesce them.
    //
    // Not sure how useful this will end up being, but it's cheap and worth
    // testing, should maybe add some metrics to see how common it is...
    //
    E820h_MemMapEntry entry0 = {.type = MEM_MAP_ENTRY_AVAILABLE,
                                .base = 0x0000000000000000,
                                .length = 0x0000000000002000,
                                .attrs = 0};
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion *region = page_alloc_init_e820(map, 0, region_buffer);
    uint64_t page1 = page_alloc(region); // will be at 0x1000
    uint64_t page2 = page_alloc(region); // will be at 0x2000

    // Stack is now empty
    munit_assert_ptr_equal(region->sp, stack_base(region));

    // Two pages total, none free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x0);

    // Free the lower page
    page_free(region, page2);

    // Stack is no longer empty
    munit_assert_ptr_equal(region->sp, region + 1);

    // Top entry is based at 0x1000, 1 pages
    munit_assert_uint64(region->sp->base, ==, 0x1000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    // Two pages total, one free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x1000);

    // Free the lower page
    page_free(region, page1);

    // Top entry now based at 0, and now 2 pages
    munit_assert_uint64(region->sp->base, ==, 0x0);
    munit_assert_uint64(region->sp->size, ==, 0x2);

    // Two pages total, two free
    munit_assert_uint64(region->size, ==, 0x2000);
    munit_assert_uint64(region->free, ==, 0x2000);

    free(map);
    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    region_buffer = malloc(0x100000);
    return NULL;
}

static void teardown(void *param) { free(region_buffer); }

static MunitTest test_suite_tests[] = {
        {(char *)"/init_empty", test_init_empty, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_invalid", test_init_all_invalid, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_reserved", test_init_all_reserved, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_acpi", test_init_all_acpi, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_acpi_nvs", test_init_all_acpi_nvs, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unusable", test_init_all_unusable, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_disabled", test_init_all_disabled, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_persistent", test_init_all_persistent, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unknown", test_init_all_unknown, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_illegal", test_init_all_illegal, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_zero_length", test_init_zero_length, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_too_small", test_init_too_small, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_one_avail", test_init_one_available, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unaligned_zero", test_init_unaligned_zero, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unaligned_one", test_init_unaligned_one, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_some_avail", test_init_some_available, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_1M_at_zero", test_init_1M_at_zero, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_big_region", test_init_large_region, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_regions", test_init_two_regions, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_big_regions", test_init_two_large_regions, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_split_regions", test_init_two_noncontig_regions,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_unequal_regions", test_init_two_unequal_regions,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc_page_empty", test_alloc_page_empty, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_page", test_alloc_page, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_two_pages", test_alloc_two_pages, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_from_two_blocks", test_alloc_two_blocks, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/alloc_m_empty_one", test_alloc_page_m_empty_one, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_empty_two", test_alloc_page_m_empty_two, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_one", test_alloc_page_m_one, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_one_from_one", test_alloc_page_m_one_from_one, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_two_from_one", test_alloc_page_m_two_from_one, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_two_from_two", test_alloc_page_m_two_from_two, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_not_top_split", test_alloc_page_m_not_top_split,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_not_top_remove", test_alloc_page_m_not_top_remove,
         setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/alloc_m_top_remove", test_alloc_page_m_top_remove, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/free_page", test_free_page, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free_unaligned", test_free_unaligned_page, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free_contig_fwd", test_free_contig_pages_forward, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/free_contig_bwd", test_free_contig_pages_backward, setup,
         teardown, MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/pmm/stack", test_suite_tests,
                                      NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}