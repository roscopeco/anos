/*
 * Tests for RISC-V virtual memory mapper implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * TODO this isn't running as part of the build yet, 
 * need to sort out test infra for ARCH
 */

#include <stddef.h>
#include <stdint.h>
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
#define DIRECT_MAP_BASE 0xffff800000000000ULL
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))
#define VM_PAGE_SIZE PAGE_SIZE

// Simple definition for SpinLock
typedef int SpinLock;

// Page table entry flags for VMM
#define PG_PRESENT (1ULL << 0)  // Valid
#define PG_READ (1ULL << 1)     // Read
#define PG_WRITE (1ULL << 2)    // Write
#define PG_EXEC (1ULL << 3)     // Execute
#define PG_USER (1ULL << 4)     // User
#define PG_GLOBAL (1ULL << 5)   // Global
#define PG_ACCESSED (1ULL << 6) // Accessed
#define PG_DIRTY (1ULL << 7)    // Dirty

// Add memclr implementation for tests
static inline void memclr(void *ptr, size_t size) { memset(ptr, 0, size); }

// Memory region structure needed for page allocation
typedef struct {
    uint64_t flags;
    uint64_t size;
    uint64_t free;
    void *sp;
} MemoryRegion;

// Forward declarations for the functions we want to test
typedef enum {
    PT_LEVEL_PML4 = 4,
    PT_LEVEL_PDPT = 3,
    PT_LEVEL_PD = 2,
    PT_LEVEL_PT = 1,
} PagetableLevel;

// Uncomment for testing without UNIT_TESTS defined
// #define STATIC_EXCEPT_TESTS
#ifdef STATIC_EXCEPT_TESTS
#undef STATIC_EXCEPT_TESTS
#define STATIC_EXCEPT_TESTS
#endif

// External functions from vmmapper.c we're testing
bool is_leaf(uint64_t table_entry);
uint64_t *ensure_tables(uint64_t *root_table, uintptr_t virt_addr, PagetableLevel to_level);
extern bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t virt_addr, uint64_t phys_addr, uint16_t flags);
extern bool vmm_map_page_in(uint64_t *pml4, uintptr_t virt_addr, uint64_t page, uint16_t flags);
extern bool vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr, uint16_t flags);
extern bool vmm_map_page(uintptr_t virt_addr, uint64_t page, uint16_t flags);
extern uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr);
extern uintptr_t vmm_unmap_page(uintptr_t virt_addr);
extern void vmm_invalidate_page(uintptr_t virt_addr);

// Mock implementations
MemoryRegion *physical_region;
static SpinLock mock_spinlock;

static uintptr_t mock_allocated_pages[100]; // Store allocated pages for verification
static int mock_allocated_pages_count = 0;

// Mock CPU functions
uint64_t cpu_read_satp() {
    // Return a mock SATP value that would point to our mock PML4
    return 0x8000000000000001; // Mode = Sv39 (8), ASID = 0, PPN = 1
}

void cpu_invalidate_tlb_addr(uintptr_t virt_addr) {
    // Mock implementation - just record that invalidation was called
}

// Mock spinlock functions
uint64_t spinlock_lock_irqsave(SpinLock *lock) {
    return 0x42; // Mock flags
}

void spinlock_unlock_irqrestore(SpinLock *lock, uint64_t flags) {
    // Do nothing in the mock
}

// Mock page allocation
uintptr_t page_alloc(MemoryRegion *region) {
    // Return a mock page address and record it
    uintptr_t page = 0x1000 * (mock_allocated_pages_count + 1);
    mock_allocated_pages[mock_allocated_pages_count++] = page;
    return page;
}

// Helper to convert from virt to phys and vice versa
static inline uintptr_t vmm_phys_to_virt(uintptr_t phys_addr) { return DIRECT_MAP_BASE + phys_addr; }

static inline void *vmm_phys_to_virt_ptr(uintptr_t phys_addr) { return (void *)vmm_phys_to_virt(phys_addr); }

static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) { return virt_addr - DIRECT_MAP_BASE; }

// Helper for page table entry conversions
static inline uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys, uint64_t flags) {
    return ((phys & ~0xFFF) >> 2) | flags;
}

static inline uintptr_t vmm_table_entry_to_phys(uintptr_t table_entry) { return ((table_entry >> 10) << 12); }

// Helper functions for table indexing
static inline uint16_t vmm_virt_to_table_index(uintptr_t virt_addr, uint8_t level) {
    return ((virt_addr >> ((9 * (level - 1)) + 12)) & 0x1ff);
}

static inline uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 9 + 9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr) { return ((virt_addr >> (9 + 12)) & 0x1ff); }

static inline uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr) { return ((virt_addr >> 12) & 0x1ff); }

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

// Test for the is_leaf function
static MunitResult test_is_leaf(const MunitParameter params[], void *fixture) {
    // Test case 1: Entry with no leaf bits
    uint64_t non_leaf_entry = PG_PRESENT | PG_USER;
    munit_assert_false(is_leaf(non_leaf_entry));

    // Test case 2: Entry with READ flag (leaf)
    uint64_t read_leaf_entry = PG_PRESENT | PG_READ;
    munit_assert_true(is_leaf(read_leaf_entry));

    // Test case 3: Entry with WRITE flag (leaf)
    uint64_t write_leaf_entry = PG_PRESENT | PG_WRITE;
    munit_assert_true(is_leaf(write_leaf_entry));

    // Test case 4: Entry with EXEC flag (leaf)
    uint64_t exec_leaf_entry = PG_PRESENT | PG_EXEC;
    munit_assert_true(is_leaf(exec_leaf_entry));

    // Test case 5: Entry with all leaf flags
    uint64_t all_leaf_entry = PG_PRESENT | PG_READ | PG_WRITE | PG_EXEC;
    munit_assert_true(is_leaf(all_leaf_entry));

    return MUNIT_OK;
}

// Test for ensure_tables with valid inputs
static MunitResult test_ensure_tables_valid(const MunitParameter params[], void *fixture) {
    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Test ensure_tables to PT level (level 1)
    uintptr_t virt_addr = 0xffff900000001000; // Some virtual address
    uint64_t *pt = ensure_tables(pml4, virt_addr, PT_LEVEL_PT);

    // Verify that the function returned a valid pointer
    munit_assert_not_null(pt);

    // Verify that pages were allocated for the tables
    munit_assert_int(mock_allocated_pages_count, >=,
                     3); // Should allocate PDPT, PD, PT

    // Check if PML4 entry has been set up correctly
    uint16_t pml4_index = vmm_virt_to_pml4_index(virt_addr);
    munit_assert_uint64(pml4[pml4_index] & PG_PRESENT, !=, 0);

    // Cleanup
    free(pml4);

    return MUNIT_OK;
}

// Test for ensure_tables with invalid level
static MunitResult test_ensure_tables_invalid_level(const MunitParameter params[], void *fixture) {
    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Test with invalid level (0)
    uint64_t *result0 = ensure_tables(pml4, 0xffff900000001000, 0);
    munit_assert_null(result0);

    // Test with invalid level (5)
    uint64_t *result5 = ensure_tables(pml4, 0xffff900000001000, 5);
    munit_assert_null(result5);

    // Cleanup
    free(pml4);

    return MUNIT_OK;
}

// Test for vmm_map_page_containing_in and vmm_map_page_in
static MunitResult test_map_page_in(const MunitParameter params[], void *fixture) {
    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Test mapping a page with vmm_map_page_in
    uintptr_t virt_addr = 0xffff900000001000;
    uintptr_t phys_addr = 0x2000;
    uint16_t flags = PG_PRESENT | PG_READ | PG_WRITE;

    bool result = vmm_map_page_in(pml4, virt_addr, phys_addr, flags);

    // Verify that mapping succeeded
    munit_assert_true(result);

    // Verify that pages were allocated for tables
    munit_assert_int(mock_allocated_pages_count, >, 0);

    // Reset state
    mock_vmm_reset();
    mock_allocated_pages_count = 0;

    // Test mapping a page with vmm_map_page_containing_in
    virt_addr = 0xffff900000002000;
    phys_addr = 0x3500; // Not page-aligned

    result = vmm_map_page_containing_in(pml4, virt_addr, phys_addr, flags);

    // Verify that mapping succeeded
    munit_assert_true(result);

    // Verify that pages were allocated for tables
    munit_assert_int(mock_allocated_pages_count, >, 0);

    // Cleanup
    free(pml4);

    return MUNIT_OK;
}

// Test for vmm_unmap_page_in with different levels of pages
static MunitResult test_unmap_page_in(const MunitParameter params[], void *fixture) {
    // Create a mock PML4 table
    uint64_t *pml4 = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));

    // Set up a test mapping scenario
    uintptr_t virt_addr = 0xffff900000001000;
    uint16_t pml4_index = vmm_virt_to_pml4_index(virt_addr);

    // Test case 1: PML4 leaf entry (terapage)
    pml4[pml4_index] = vmm_phys_and_flags_to_table_entry(0x1000, PG_PRESENT | PG_READ);

    uintptr_t result = vmm_unmap_page_in(pml4, virt_addr);

    // Verify that unmapping returned the correct physical address
    munit_assert_uint64(result, ==, 0x1000);

    // Verify that PML4 entry was cleared
    munit_assert_uint64(pml4[pml4_index], ==, 0);

    // Create nested tables for the next test
    uint64_t *pdpt = calloc(PAGE_TABLE_ENTRIES, sizeof(uint64_t));
    pml4[pml4_index] = vmm_phys_and_flags_to_table_entry((uintptr_t)pdpt, PG_PRESENT);

    uint16_t pdpt_index = vmm_virt_to_pdpt_index(virt_addr);

    // Test case 2: PDPT leaf entry (gigapage)
    pdpt[pdpt_index] = vmm_phys_and_flags_to_table_entry(0x2000, PG_PRESENT | PG_READ);

    result = vmm_unmap_page_in(pml4, virt_addr);

    // Verify that unmapping returned the correct physical address
    munit_assert_uint64(result, ==, 0x2000);

    // Verify that PDPT entry was cleared
    munit_assert_uint64(pdpt[pdpt_index], ==, 0);

    // Cleanup
    free(pdpt);
    free(pml4);

    return MUNIT_OK;
}

// Test vmm_map_page and vmm_unmap_page (the global functions)
static MunitResult test_map_unmap_page_global(const MunitParameter params[], void *fixture) {
    // We'll use the mock VMM functions to verify calls
    uintptr_t virt_addr = 0xffff900000005000;
    uintptr_t phys_addr = 0x5000;
    uint16_t flags = PG_PRESENT | PG_READ | PG_WRITE | PG_USER;

    // Reset mock state
    mock_vmm_reset();

    // Test mapping a page
    bool result = vmm_map_page(virt_addr, phys_addr, flags);

    // Verify that mapping succeeded
    munit_assert_true(result);

    // Verify that the mock received the correct parameters
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==, virt_addr);
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==, phys_addr);
    munit_assert_uint16(mock_vmm_get_last_page_map_flags(), ==, flags);

    // Test unmapping the page
    uintptr_t unmap_result = vmm_unmap_page(virt_addr);

    // Verify that the unmapping tracked the correct address
    munit_assert_uint64(mock_vmm_get_last_page_unmap_virt(), ==, virt_addr);

    return MUNIT_OK;
}

// Define the test suite
static MunitTest test_suite_tests[] = {
        {"/is_leaf", test_is_leaf, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ensure_tables_valid", test_ensure_tables_valid, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/ensure_tables_invalid_level", test_ensure_tables_invalid_level, test_setup, test_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/map_page_in", test_map_page_in, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/unmap_page_in", test_unmap_page_in, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/map_unmap_page_global", test_map_unmap_page_global, test_setup, test_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/vmm/riscv64/vmmapper", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) { return munit_suite_main(&test_suite, NULL, argc, argv); }