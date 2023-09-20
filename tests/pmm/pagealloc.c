/*
 * Tests for the page allocator
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "munit.h"
#include "pmm/pagealloc.h"

static void* region_buffer;

static E820h_MemMap* create_mem_map(int num_entries) {
    E820h_MemMap *map = malloc(sizeof(E820h_MemMap) + sizeof(E820h_MemMapEntry) * num_entries);
    map->num_entries = num_entries;
    return map;
}

static MunitResult test_init_empty(const MunitParameter params[], void *param) {
    E820h_MemMap map = { .num_entries = 0 };

    MemoryRegion* region = page_alloc_init(&map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_init_all_invalid(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_INVALID };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_reserved(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_RESERVED };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_ACPI };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_acpi_nvs(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_ACPI_NVS };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_unusable(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_UNUSABLE };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_disabled(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_DISABLED };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_persistent(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_PERSISTENT };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_unknown(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_UNKNOWN };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_all_illegal(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = 99 };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_one_available(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0x100000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_some_available(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_RESERVED,  .base = 0x0010000000000000, .length = 0x0100000000100000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion* region = page_alloc_init(map, region_buffer);
    munit_assert_uint64(region->size, ==, 0x100000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_1M_at_zero(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_RESERVED,  .base = 0x0010000000000000, .length = 0x0100000000100000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x100000);
    munit_assert_uint64(region->free, ==, 0x100000);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_large_region(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(1);
    map->entries[0] = entry0;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x10000000);
    munit_assert_uint64(region->free, ==, 0x10000000);

    // Do we have 128 order-9 blocks with increasing (by max block size) bases?
    munit_assert_not_null(region->order_lists[9]);

    PhysicalBlock *current = region->order_lists[9];
    int count = 0;
    uint64_t expected_base = 0;

    while (current) {
      munit_assert_uint64(current->base, ==, expected_base);
      munit_assert_uint64(current->order, ==, 9);

      current = current->next;
      count++;
      expected_base += 0x200000;
    }

    munit_assert_int(count, ==, 128);   // 128 blocks, 2MiB each == 256MiB (0x10000000)

    // Are all the other order lists empty?
    munit_assert_null(region->order_lists[0]);
    munit_assert_null(region->order_lists[1]);
    munit_assert_null(region->order_lists[2]);
    munit_assert_null(region->order_lists[3]);
    munit_assert_null(region->order_lists[4]);
    munit_assert_null(region->order_lists[5]);
    munit_assert_null(region->order_lists[6]);
    munit_assert_null(region->order_lists[7]);
    munit_assert_null(region->order_lists[8]);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_regions(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_RESERVED,  .base = 0x0010000000000000, .length = 0x0100000000100000, .attrs = 0 };
    E820h_MemMapEntry entry2 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000100000, .length = 0x0000000000020000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x120000);
    munit_assert_uint64(region->free, ==, 0x120000);

    // Is there a single order-8 block at base 0x100000?
    munit_assert_not_null(region->order_lists[8]);
    munit_assert_null(region->order_lists[8]->next);
    munit_assert_uint64(region->order_lists[8]->base, ==, 0x000000);
    munit_assert_uint64(region->order_lists[8]->order, ==, 8);

    // Is there a single order-5 block at base 0x100000?
    munit_assert_not_null(region->order_lists[5]);
    munit_assert_null(region->order_lists[5]->next);
    munit_assert_uint64(region->order_lists[5]->base, ==, 0x100000);
    munit_assert_uint64(region->order_lists[5]->order, ==, 5);

    // Are all the other order lists empty?
    munit_assert_null(region->order_lists[0]);
    munit_assert_null(region->order_lists[1]);
    munit_assert_null(region->order_lists[2]);
    munit_assert_null(region->order_lists[3]);
    munit_assert_null(region->order_lists[4]);
    munit_assert_null(region->order_lists[6]);
    munit_assert_null(region->order_lists[7]);
    munit_assert_null(region->order_lists[9]);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_large_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000010000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x20000000);
    munit_assert_uint64(region->free, ==, 0x20000000);

    // Do we have 256 order-9 blocks with increasing (by max block size) bases?
    munit_assert_not_null(region->order_lists[9]);

    PhysicalBlock *current = region->order_lists[9];
    int count = 0;
    uint64_t expected_base = 0;

    while (current) {
      munit_assert_uint64(current->base, ==, expected_base);
      munit_assert_uint64(current->order, ==, 9);

      current = current->next;
      count++;
      expected_base += 0x200000;
    }

    munit_assert_int(count, ==, 256);   // 256 blocks, 2MiB each == 512MiB (0x20000000)

    // Are all the other order lists empty?
    munit_assert_null(region->order_lists[0]);
    munit_assert_null(region->order_lists[1]);
    munit_assert_null(region->order_lists[2]);
    munit_assert_null(region->order_lists[3]);
    munit_assert_null(region->order_lists[4]);
    munit_assert_null(region->order_lists[5]);
    munit_assert_null(region->order_lists[6]);
    munit_assert_null(region->order_lists[7]);
    munit_assert_null(region->order_lists[8]);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_noncontig_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000040000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x20000000);
    munit_assert_uint64(region->free, ==, 0x20000000);

    // Do we have 128 order-9 blocks with increasing (by max block size) bases, based at 0?
    munit_assert_not_null(region->order_lists[9]);

    PhysicalBlock *current = region->order_lists[9];
    int count = 0;
    uint64_t expected_base = 0;

    for (int i = 0; i < 128; i++) {
      munit_assert_uint64(current->base, ==, expected_base);
      munit_assert_uint64(current->order, ==, 9);

      current = current->next;
      count++;
      expected_base += 0x200000;
    }

    // Followed by another 128 blocks, increasing the same, but based at 0x40000000?
    expected_base = 0x40000000;

    for (int i = 0; i < 128; i++) {
      munit_assert_uint64(current->base, ==, expected_base);
      munit_assert_uint64(current->order, ==, 9);

      current = current->next;
      count++;
      expected_base += 0x200000;
    }

    // And no more?
    munit_assert_null(current);

    // Are all the other order lists empty?
    munit_assert_null(region->order_lists[0]);
    munit_assert_null(region->order_lists[1]);
    munit_assert_null(region->order_lists[2]);
    munit_assert_null(region->order_lists[3]);
    munit_assert_null(region->order_lists[4]);
    munit_assert_null(region->order_lists[5]);
    munit_assert_null(region->order_lists[6]);
    munit_assert_null(region->order_lists[7]);
    munit_assert_null(region->order_lists[8]);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_init_two_unequal_regions(const MunitParameter params[], void *param) {
    // Single 256MiB available memory area
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000010000000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000010000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(2);
    map->entries[0] = entry0;
    map->entries[1] = entry1;

    MemoryRegion* region = page_alloc_init(map, region_buffer);

    munit_assert_uint64(region->size, ==, 0x10100000);
    munit_assert_uint64(region->free, ==, 0x10100000);

    // Do we have 128 order-9 blocks with increasing (by max block size) bases, based at 0?
    munit_assert_not_null(region->order_lists[9]);

    PhysicalBlock *current = region->order_lists[9];
    int count = 0;
    uint64_t expected_base = 0;

    for (int i = 0; i < 128; i++) {
      munit_assert_uint64(current->base, ==, expected_base);
      munit_assert_uint64(current->order, ==, 9);

      current = current->next;
      count++;
      expected_base += 0x200000;
    }

    munit_assert_int(count, ==, 128);   // 256 blocks, 2MiB each == 512MiB (0x20000000)

    // And no more?
    munit_assert_null(current);

    // Also 1 block, order-8, based at 0x10000000
    expected_base = 0x10000000;
    current = region->order_lists[8];
    count = 0;

    munit_assert_uint64(current->base, ==, expected_base);
    munit_assert_uint64(current->order, ==, 8);
    munit_assert_null(current->next);

    // Are all the other order lists empty?
    munit_assert_null(region->order_lists[0]);
    munit_assert_null(region->order_lists[1]);
    munit_assert_null(region->order_lists[2]);
    munit_assert_null(region->order_lists[3]);
    munit_assert_null(region->order_lists[4]);
    munit_assert_null(region->order_lists[5]);
    munit_assert_null(region->order_lists[6]);
    munit_assert_null(region->order_lists[7]);

    free(map);
    return MUNIT_OK;
}

static MunitResult test_alloc_page(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_RESERVED,  .base = 0x0010000000000000, .length = 0x0100000000100000, .attrs = 0 };
    E820h_MemMapEntry entry2 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000100000, .length = 0x0000000000020000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    PhysPage page;
    MemoryRegion* region = page_alloc_init(map, region_buffer);

    // munit_assert_true(page_alloc_alloc_page(&page));

    // munit_assert_uint64(page.phys_addr, ==, 0);

    // munit_assert_uint64(region->size, ==, 0x120000);
    // munit_assert_uint64(region->free, ==, 0x11F000);

    return MUNIT_OK;
}

static MunitResult test_alloc_two_pages(const MunitParameter params[], void *param) {
    E820h_MemMapEntry entry0 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000000000, .length = 0x0000000000100000, .attrs = 0 };
    E820h_MemMapEntry entry1 = { .type = MEM_MAP_ENTRY_RESERVED,  .base = 0x0010000000000000, .length = 0x0100000000100000, .attrs = 0 };
    E820h_MemMapEntry entry2 = { .type = MEM_MAP_ENTRY_AVAILABLE, .base = 0x0000000000100000, .length = 0x0000000000020000, .attrs = 0 };
    E820h_MemMap *map = create_mem_map(3);
    map->entries[0] = entry0;
    map->entries[1] = entry1;
    map->entries[2] = entry2;

    PhysPage page1, page2;
    MemoryRegion* region = page_alloc_init(map, region_buffer);

    // munit_assert_true(page_alloc_alloc_page(&page1));
    // munit_assert_true(page_alloc_alloc_page(&page2));

    // munit_assert_uint64(page1.phys_addr, ==, 0);
    // munit_assert_uint64(page2.phys_addr, ==, 4096);

    // munit_assert_uint64(region->size, ==, 0x120000);
    // munit_assert_uint64(region->free, ==, 0x11E000);

    return MUNIT_OK;
}

static void* setup(const MunitParameter params[], void* user_data) {
    region_buffer = malloc(0x100000);
    return NULL;
}

static void teardown(void *param) {
    free(region_buffer);
}

static MunitTest test_suite_tests[] = {
  { (char*) "/pmm/palloc/init_empty", test_init_empty, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_invalid", test_init_all_invalid, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_reserved", test_init_all_reserved, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_acpi", test_init_all_acpi, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_acpi_nvs", test_init_all_acpi_nvs, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_unusable", test_init_all_unusable, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_disabled", test_init_all_disabled, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_persistent", test_init_all_persistent, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_unknown", test_init_all_unknown, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_illegal", test_init_all_illegal, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_one_avail", test_init_one_available, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_some_avail", test_init_some_available, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_1M_at_zero", test_init_1M_at_zero, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_big_regions", test_init_large_region, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_two_regions", test_init_two_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_two_big_regions", test_init_two_large_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_two_split_regions", test_init_two_noncontig_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/init_two_unequal_regions", test_init_two_unequal_regions, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },

  { (char*) "/pmm/palloc/alloc_page", test_alloc_page, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/pmm/palloc/alloc_two_pages", test_alloc_two_pages, setup, teardown, MUNIT_TEST_OPTION_NONE, NULL },

  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite test_suite = {
  (char*) "",
  test_suite_tests,
  NULL,
  1,
  MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}