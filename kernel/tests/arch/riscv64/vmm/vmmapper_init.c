/*
 * Tests for RISC-V direct mapping initialization
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO this isn't running as part of the build yet, 
 * need to sort out test infra for ARCH
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "munit.h"

// Include mocks
#include "mock_machine.h"
#include "mock_riscv64_vmm.h"
#include "mock_vmm.h"

// Define required constants for the test
#define PAGE_SIZE ((uintptr_t)(0x1000))            // 4KB
#define MEGA_PAGE_SIZE ((uintptr_t)(0x200000))     // 2MB
#define GIGA_PAGE_SIZE ((uintptr_t)(0x40000000))   // 1GB
#define TERA_PAGE_SIZE ((uintptr_t)(0x8000000000)) // 512GB
#define PAGE_TABLE_ENTRIES 512
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))
#define TERAPAGE_ALIGN_MASK (~(TERA_PAGE_SIZE - 1))

// Page table entry flags for VMM
#define PG_PRESENT (1ULL << 0)  // Valid
#define PG_READ (1ULL << 1)     // Read
#define PG_WRITE (1ULL << 2)    // Write
#define PG_EXEC (1ULL << 3)     // Execute
#define PG_USER (1ULL << 4)     // User
#define PG_GLOBAL (1ULL << 5)   // Global
#define PG_ACCESSED (1ULL << 6) // Accessed
#define PG_DIRTY (1ULL << 7)    // Dirty

// Memory map entry types from machine.h
typedef enum {
    LIMINE_MEMMAP_USABLE = 0,
    LIMINE_MEMMAP_RESERVED = 1,
    LIMINE_MEMMAP_ACPI_RECLAIMABLE = 2,
    LIMINE_MEMMAP_ACPI_NVS = 3,
    LIMINE_MEMMAP_BAD_MEMORY = 4,
    LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE = 5,
    LIMINE_MEMMAP_EXECUTABLE_AND_MODULES = 6,
    LIMINE_MEMMAP_FRAMEBUFFER = 7,
} Limine_MemMapEntry_Type;

// Forward declarations for function signatures
typedef struct {
    uint64_t base;
    uint64_t length;
    uint64_t type;
} __attribute__((packed)) Limine_MemMapEntry;

typedef struct {
    uint64_t revision;
    uint64_t entry_count;
    Limine_MemMapEntry **entries;
} __attribute__((packed)) Limine_MemMap;

// Define MemoryRegion for tests
typedef struct {
    uint64_t flags;
    uint64_t size;
    uint64_t free;
    void *sp;
} MemoryRegion;

// External functions from the file we want to test
extern void vmm_init_direct_mapping(uint64_t *pml4, Limine_MemMap *memmap);

// Mock implementations for the functions used by vmm_init_direct_mapping
MemoryRegion *physical_region;
static uintptr_t mock_allocated_pages[100]; // Store allocated pages for verification
static int mock_allocated_pages_count = 0;

// Mock for page allocation
uintptr_t page_alloc(MemoryRegion *region) {
    // Return a mock page address and record it
    uintptr_t page = 0x1000 * (mock_allocated_pages_count + 1);
    mock_allocated_pages[mock_allocated_pages_count++] = page;
    return page;
}

// Helper to convert from virt to phys and vice versa
static inline uintptr_t vmm_phys_to_virt(uintptr_t phys_addr) { return DIRECT_MAP_BASE + phys_addr; }

static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) { return virt_addr - DIRECT_MAP_BASE; }

// Helper for page table entry conversions
static inline uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys, uint64_t flags) {
    return ((phys & ~0xFFF) >> 2) | flags;
}

static inline uintptr_t vmm_table_entry_to_phys(uintptr_t table_entry) { return ((table_entry >> 10) << 12); }

// Helper functions for table indexing
static inline uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 9 + 9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr) { return ((virt_addr >> 12) & 0x1ff); }

// Function to check if direct mapping would be created for a specific address
bool is_direct_mapped(uintptr_t phys_addr) {
    uintptr_t virt_addr = vmm_phys_to_virt(phys_addr);
    return mock_vmm_is_page_mapped(virt_addr);
}

// Setup function for tests
static void *test_setup(const MunitParameter params[], void *user_data) {
    // Initialize physical_region
    physical_region = malloc(sizeof(MemoryRegion));
    memset(physical_region, 0, sizeof(MemoryRegion));

    // Reset the page allocation counter
    mock_allocated_pages_count = 0;
    memset(mock_allocated_pages, 0, sizeof(mock_allocated_pages));

    // Reset VMM mock state
    mock_vmm_reset();

    return NULL;
}

// Teardown function for tests
static void test_teardown(void *fixture) {
    free(physical_region);
    physical_region = NULL;
}

// Test for mapping a single usable memory region
static MunitResult test_single_usable_region(const MunitParameter params[], void *fixture) {
    // Create a simple memory map with one usable region
    Limine_MemMapEntry entry = {.base = 0x1000,
                                .length = PAGE_SIZE * 10, // 10 pages
                                .type = LIMINE_MEMMAP_USABLE};

    Limine_MemMapEntry *entry_ptr = malloc(sizeof(Limine_MemMapEntry));
    memcpy(entry_ptr, &entry, sizeof(Limine_MemMapEntry));

    Limine_MemMapEntry **entries = malloc(sizeof(Limine_MemMapEntry *));
    entries[0] = entry_ptr;

    Limine_MemMap memmap = {.revision = 1, .entry_count = 1, .entries = entries};

    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Initialize the direct mapping
    vmm_init_direct_mapping(pml4, &memmap);

    // Verify that page tables were allocated
    munit_assert_int(mock_allocated_pages_count, >, 0);

    // Check if the middle of the region is mapped
    uintptr_t middle_phys = 0x1000 + (PAGE_SIZE * 5);
    uintptr_t middle_virt = vmm_phys_to_virt(middle_phys);
    munit_assert_true(mock_vmm_is_page_mapped(middle_virt));

    // Check flags - should be present, read, write and global for usable memory
    uint16_t expected_flags = PG_PRESENT | PG_READ | PG_WRITE | PG_GLOBAL;
    uint16_t flags = mock_vmm_get_flags_for_virt(middle_virt);
    munit_assert_uint16(flags & expected_flags, ==, expected_flags);

    // Cleanup
    free(pml4);
    free(entry_ptr);
    free(entries);

    return MUNIT_OK;
}

// Test for mapping multiple memory regions of different types
static MunitResult test_multiple_regions(const MunitParameter params[], void *fixture) {
    // Create a memory map with different types of regions
    Limine_MemMapEntry entries_data[4] = {{.base = 0x1000,
                                           .length = PAGE_SIZE * 5, // 5 pages
                                           .type = LIMINE_MEMMAP_USABLE},
                                          {
                                                  .base = 0x10000,
                                                  .length = PAGE_SIZE * 3,       // 3 pages
                                                  .type = LIMINE_MEMMAP_RESERVED // Should be ignored
                                          },
                                          {.base = 0x20000,
                                           .length = MEGA_PAGE_SIZE, // 1 megapage
                                           .type = LIMINE_MEMMAP_ACPI_RECLAIMABLE},
                                          {.base = 0x400000,
                                           .length = GIGA_PAGE_SIZE, // 1 gigapage
                                           .type = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE}};

    Limine_MemMapEntry *entry_ptrs[4];
    for (int i = 0; i < 4; i++) {
        entry_ptrs[i] = malloc(sizeof(Limine_MemMapEntry));
        memcpy(entry_ptrs[i], &entries_data[i], sizeof(Limine_MemMapEntry));
    }

    Limine_MemMapEntry **entries = malloc(4 * sizeof(Limine_MemMapEntry *));
    for (int i = 0; i < 4; i++) {
        entries[i] = entry_ptrs[i];
    }

    Limine_MemMap memmap = {.revision = 1, .entry_count = 4, .entries = entries};

    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Initialize the direct mapping
    vmm_init_direct_mapping(pml4, &memmap);

    // Verify that we allocated page tables
    munit_assert_int(mock_allocated_pages_count, >, 0);

    // Check if regions of different types were correctly mapped

    // 1. Usable region should be mapped with read/write permissions
    uintptr_t usable_phys = 0x3000; // In the middle of first region
    uintptr_t usable_virt = vmm_phys_to_virt(usable_phys);
    munit_assert_true(mock_vmm_is_page_mapped(usable_virt));
    uint16_t usable_flags = mock_vmm_get_flags_for_virt(usable_virt);
    munit_assert_uint16(usable_flags & (PG_PRESENT | PG_READ | PG_WRITE), ==, (PG_PRESENT | PG_READ | PG_WRITE));

    // 2. Reserved region should not be mapped
    uintptr_t reserved_phys = 0x11000; // In the middle of reserved region
    uintptr_t reserved_virt = vmm_phys_to_virt(reserved_phys);
    munit_assert_false(mock_vmm_is_page_mapped(reserved_virt));

    // 3. ACPI Reclaimable should be mapped and writeable
    uintptr_t acpi_phys = 0x100000; // In the middle of ACPI region
    uintptr_t acpi_virt = vmm_phys_to_virt(acpi_phys);
    munit_assert_true(mock_vmm_is_page_mapped(acpi_virt));
    uint16_t acpi_flags = mock_vmm_get_flags_for_virt(acpi_virt);
    munit_assert_uint16(acpi_flags & (PG_PRESENT | PG_READ | PG_WRITE), ==, (PG_PRESENT | PG_READ | PG_WRITE));

    // 4. Bootloader reclaimable should be mapped and writeable
    uintptr_t boot_phys = 0x800000; // In the middle of bootloader region
    uintptr_t boot_virt = vmm_phys_to_virt(boot_phys);
    munit_assert_true(mock_vmm_is_page_mapped(boot_virt));
    uint16_t boot_flags = mock_vmm_get_flags_for_virt(boot_virt);
    munit_assert_uint16(boot_flags & (PG_PRESENT | PG_READ | PG_WRITE), ==, (PG_PRESENT | PG_READ | PG_WRITE));

    // Cleanup
    free(pml4);
    for (int i = 0; i < 4; i++) {
        free(entry_ptrs[i]);
    }
    free(entries);

    return MUNIT_OK;
}

// Test with different page sizes
static MunitResult test_different_page_sizes(const MunitParameter params[], void *fixture) {
    // Create a memory map with regions naturally aligned to different page sizes
    Limine_MemMapEntry entries_data[3] = {{// Regular page-aligned region
                                           .base = PAGE_SIZE,
                                           .length = PAGE_SIZE * 4,
                                           .type = LIMINE_MEMMAP_USABLE},
                                          {// Megapage-aligned region
                                           .base = MEGA_PAGE_SIZE,
                                           .length = MEGA_PAGE_SIZE * 2,
                                           .type = LIMINE_MEMMAP_USABLE},
                                          {// Gigapage-aligned region
                                           .base = GIGA_PAGE_SIZE,
                                           .length = GIGA_PAGE_SIZE,
                                           .type = LIMINE_MEMMAP_USABLE}};

    Limine_MemMapEntry *entry_ptrs[3];
    for (int i = 0; i < 3; i++) {
        entry_ptrs[i] = malloc(sizeof(Limine_MemMapEntry));
        memcpy(entry_ptrs[i], &entries_data[i], sizeof(Limine_MemMapEntry));
    }

    Limine_MemMapEntry **entries = malloc(3 * sizeof(Limine_MemMapEntry *));
    for (int i = 0; i < 3; i++) {
        entries[i] = entry_ptrs[i];
    }

    Limine_MemMap memmap = {.revision = 1, .entry_count = 3, .entries = entries};

    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Initialize the direct mapping
    vmm_init_direct_mapping(pml4, &memmap);

    // 1. Verify regular page mapping
    uintptr_t reg_page_phys = PAGE_SIZE * 2;
    uintptr_t reg_page_virt = vmm_phys_to_virt(reg_page_phys);
    munit_assert_true(mock_vmm_is_page_mapped(reg_page_virt));

    // 2. Verify megapage mapping
    uintptr_t mega_page_phys = MEGA_PAGE_SIZE * 1.5; // Middle of the megapage region
    uintptr_t mega_page_virt = vmm_phys_to_virt(mega_page_phys);
    munit_assert_true(mock_vmm_is_page_mapped(mega_page_virt));

    // 3. Verify gigapage mapping
    uintptr_t giga_page_phys = GIGA_PAGE_SIZE * 1.25; // Within the gigapage
    uintptr_t giga_page_virt = vmm_phys_to_virt(giga_page_phys);
    munit_assert_true(mock_vmm_is_page_mapped(giga_page_virt));

    // Cleanup
    free(pml4);
    for (int i = 0; i < 3; i++) {
        free(entry_ptrs[i]);
    }
    free(entries);

    return MUNIT_OK;
}

// Test with an overflowing address (beyond MAX_PHYS_ADDR)
static MunitResult test_address_overflow(const MunitParameter params[], void *fixture) {
    // Create a memory map with a region that extends beyond MAX_PHYS_ADDR
    Limine_MemMapEntry entry = {.base = (127ULL * 1024 * 1024 * 1024 * 1024) - PAGE_SIZE, // Just below MAX_PHYS_ADDR
                                .length = PAGE_SIZE * 2, // Extends beyond MAX_PHYS_ADDR
                                .type = LIMINE_MEMMAP_USABLE};

    Limine_MemMapEntry *entry_ptr = malloc(sizeof(Limine_MemMapEntry));
    memcpy(entry_ptr, &entry, sizeof(Limine_MemMapEntry));

    Limine_MemMapEntry **entries = malloc(sizeof(Limine_MemMapEntry *));
    entries[0] = entry_ptr;

    Limine_MemMap memmap = {.revision = 1, .entry_count = 1, .entries = entries};

    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Initialize the direct mapping - this should handle the overflow gracefully
    vmm_init_direct_mapping(pml4, &memmap);

    // We'll still have page tables allocated for the valid part
    munit_assert_int(mock_allocated_pages_count, >, 0);

    // The valid part should be mapped
    uintptr_t valid_phys = entry.base;
    uintptr_t valid_virt = vmm_phys_to_virt(valid_phys);
    munit_assert_true(mock_vmm_is_page_mapped(valid_virt));

    // The overflowing part should NOT be mapped
    uintptr_t invalid_phys = (127ULL * 1024 * 1024 * 1024 * 1024) + PAGE_SIZE;
    uintptr_t invalid_virt = vmm_phys_to_virt(invalid_phys);
    munit_assert_false(mock_vmm_is_page_mapped(invalid_virt));

    // Cleanup
    free(pml4);
    free(entry_ptr);
    free(entries);

    return MUNIT_OK;
}

// Define the test suite
static MunitTest test_suite_tests[] = {
        {"/single_usable_region", test_single_usable_region, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/multiple_regions", test_multiple_regions, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/different_page_sizes", test_different_page_sizes, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/address_overflow", test_address_overflow, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/vmm/riscv64/direct_mapping", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) { return munit_suite_main(&test_suite, NULL, argc, argv); }