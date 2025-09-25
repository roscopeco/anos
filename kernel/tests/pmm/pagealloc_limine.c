/*
 * Tests for the page allocator init from Limine tabls
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "munit.h"
#include "pmm/pagealloc.h"

static void *region_buffer;

static Limine_MemMap *create_mem_map(int entry_count) {
    Limine_MemMap *map = munit_malloc(sizeof(Limine_MemMap) + sizeof(Limine_MemMapEntry) * entry_count);
    map->entry_count = entry_count;
    return map;
}

static MemoryBlock *stack_base(MemoryRegion *region) { return ((MemoryBlock *)(region + 1)) - 1; }

static MunitResult test_init_empty(const MunitParameter params[], void *param) {
    Limine_MemMap map = {.entry_count = 0};

    MemoryRegion *region = page_alloc_init_limine(&map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    return MUNIT_OK;
}

static MunitResult test_init_all_invalid(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_BAD_MEMORY};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_reserved(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_RESERVED};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_ACPI_RECLAIMABLE};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi_nvs(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_ACPI_NVS};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_illegal(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = 99};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_zero_length(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000000000};
    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_too_small(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000000400};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_one_available(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_unaligned_zero(const MunitParameter params[], void *param) {
    // One block, unaligned. When aligned, it will give us zero bytes
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000400, .length = 0x0000000000001080};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x0);

    // empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_unaligned_one(const MunitParameter params[], void *param) {
    // One block, unaligned. When aligned, it will give us 4KiB
    Limine_MemMapEntry entry = {.type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000400, .length = 0x0000000000002080};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x1000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0x1000);
    munit_assert_uint64(region->sp->size, ==, 0x1);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_some_available(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_RESERVED, .base = 0x0010000000000000, .length = 0x0100000000100000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[2] = {&entry0, &entry1};
    Limine_MemMap *map = create_mem_map(2);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_1M_at_zero(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_RESERVED, .base = 0x0010000000000000, .length = 0x0100000000100000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[2] = {&entry0, &entry1};
    Limine_MemMap *map = create_mem_map(2);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

    munit_assert_uint64(region->size, ==, 0x100000);
    munit_assert_uint64(region->free, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_large_region(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000010000000};

    Limine_MemMapEntry *entries[1] = {&entry0};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

    munit_assert_uint64(region->size, ==, 0x10000000);
    munit_assert_uint64(region->free, ==, 0x10000000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x10000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_regions(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000000100000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_RESERVED, .base = 0x0010000000000000, .length = 0x0100000000100000};
    Limine_MemMapEntry entry2 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000100000, .length = 0x0000000000020000};

    Limine_MemMapEntry *entries[3] = {&entry0, &entry1, &entry2};
    Limine_MemMap *map = create_mem_map(3);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

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

static MunitResult test_init_two_large_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000010000000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000010000000, .length = 0x0000000010000000};

    Limine_MemMapEntry *entries[2] = {&entry0, &entry1};
    Limine_MemMap *map = create_mem_map(2);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

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

static MunitResult test_init_two_noncontig_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000010000000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000040000000, .length = 0x0000000010000000};

    Limine_MemMapEntry *entries[2] = {&entry0, &entry1};
    Limine_MemMap *map = create_mem_map(2);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

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

static MunitResult test_init_two_unequal_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    Limine_MemMapEntry entry0 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000000000000, .length = 0x0000000010000000};
    Limine_MemMapEntry entry1 = {
            .type = LIMINE_MEMMAP_USABLE, .base = 0x0000000010000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[2] = {&entry0, &entry1};
    Limine_MemMap *map = create_mem_map(2);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);

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

static MunitResult test_init_one_executable_ignored(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {
            .type = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x0);

    // Empty stack
    munit_assert_ptr_equal(region->sp, stack_base(region));

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_one_executable_reclaimed(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {
            .type = LIMINE_MEMMAP_EXECUTABLE_AND_MODULES, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, true);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_one_bootloader_reclaimable(const MunitParameter params[], void *param) {
    Limine_MemMapEntry entry = {
            .type = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE, .base = 0x0000000000000000, .length = 0x0000000000100000};

    Limine_MemMapEntry *entries[1] = {&entry};
    Limine_MemMap *map = create_mem_map(1);
    map->entries = entries;

    MemoryRegion *region = page_alloc_init_limine(map, 0, region_buffer, false);
    munit_assert_uint64(region->size, ==, 0x100000);

    // One entry on stack
    munit_assert_ptr_equal(region->sp, stack_base(region) + 1);
    munit_assert_uint64(region->sp->base, ==, 0);
    munit_assert_uint64(region->sp->size, ==, 0x100);

    free(map);
    return MUNIT_OK;
}

static void *setup(const MunitParameter params[], void *user_data) {
    region_buffer = malloc(0x100000);
    return NULL;
}

static void teardown(void *param) { free(region_buffer); }

static MunitTest test_suite_tests[] = {
        {(char *)"/init_empty", test_init_empty, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_invalid", test_init_all_invalid, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_reserved", test_init_all_reserved, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_acpi", test_init_all_acpi, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_acpi_nvs", test_init_all_acpi_nvs, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_illegal", test_init_all_illegal, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_zero_length", test_init_zero_length, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_too_small", test_init_too_small, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_one_avail", test_init_one_available, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unaligned_zero", test_init_unaligned_zero, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_unaligned_one", test_init_unaligned_one, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_some_avail", test_init_some_available, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_1M_at_zero", test_init_1M_at_zero, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_big_region", test_init_large_region, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_regions", test_init_two_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_big_regions", test_init_two_large_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_two_split_regions", test_init_two_noncontig_regions, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/init_two_unequal_regions", test_init_two_unequal_regions, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/init_one_exec_ignore", test_init_one_executable_ignored, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {(char *)"/init_one_exec_reclaimed", test_init_one_executable_reclaimed, setup, teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/init_one_avail", test_init_one_bootloader_reclaimable, setup, teardown, MUNIT_TEST_OPTION_NONE,
         NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"/pmm/stack_limine", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}