/*
 * RISC-V Memory Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H
#define __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H

#include <stdbool.h>
#include <stdint.h>

// Low physical memory used must be mapped here, the
// mapper expects to be able to access pages
// under this...
static const uintptr_t STATIC_KERNEL_SPACE = 0xFFFFFFFF80000000;

// Direct mapping base address (from MemoryMap.md)
static const uintptr_t DIRECT_MAP_BASE = 0xffff800000000000;

// Page size constants
static const uintptr_t PAGE_SIZE = 0x1000;            // 4KB
static const uintptr_t MEGA_PAGE_SIZE = 0x200000;     // 2MB
static const uintptr_t GIGA_PAGE_SIZE = 0x40000000;   // 1GB
static const uintptr_t TERA_PAGE_SIZE = 0x8000000000; // 512GB

// Page table entry flags
#define PG_PRESENT (1ULL << 0)  // Valid
#define PG_READ (1ULL << 1)     // Read
#define PG_WRITE (1ULL << 2)    // Write
#define PG_EXEC (1ULL << 3)     // Execute
#define PG_USER (1ULL << 4)     // User
#define PG_GLOBAL (1ULL << 5)   // Global
#define PG_ACCESSED (1ULL << 6) // Accessed
#define PG_DIRTY (1ULL << 7)    // Dirty

// Page table structure
typedef struct {
    uint64_t entries[512];
} PageTable;

// For Sv39, we have 3 levels: L2, L1, L0
// For Sv48, we have 4 levels: L3, L2, L1, L0

#ifdef RISCV_SV48
// Sv48 page table structure
typedef struct {
    PageTable *l3; // Top level
    PageTable *l2; // Second level
    PageTable *l1; // Third level
    PageTable *l0; // Fourth level
} PageTableHierarchy;
#else
// Sv39 page table structure
typedef struct {
    PageTable *l2; // Top level
    PageTable *l1; // Second level
    PageTable *l0; // Third level
} PageTableHierarchy;
#endif

// Convert physical address to direct-mapped virtual address
static inline uintptr_t phys_to_virt(uintptr_t phys_addr) {
    return DIRECT_MAP_BASE + phys_addr;
}

// Convert direct-mapped virtual address to physical address
static inline uintptr_t virt_to_phys(uintptr_t virt_addr) {
    return virt_addr - DIRECT_MAP_BASE;
}

// Check if an address is in the direct mapping region
static inline bool is_direct_mapped(uintptr_t virt_addr) {
    return (virt_addr >= DIRECT_MAP_BASE) &&
           (virt_addr < DIRECT_MAP_BASE + 0x7FFFFFFFFF); // 127TB
}

// Get the page table entry for a given virtual address
// Returns NULL if the page table entry doesn't exist
uint64_t *vmm_get_pte(uintptr_t virt_addr);

// Get the page table for a given virtual address
// Returns NULL if the page table doesn't exist
PageTable *vmm_get_pt(uintptr_t virt_addr);

// Get the page directory for a given virtual address
// Returns NULL if the page directory doesn't exist
PageTable *vmm_get_pd(uintptr_t virt_addr);

// Get the page directory pointer table for a given virtual address
// Returns NULL if the page directory pointer table doesn't exist
PageTable *vmm_get_pdpt(uintptr_t virt_addr);

#ifdef RISCV_SV48
// Get the page map level 4 for a given virtual address
// Returns NULL if the page map level 4 doesn't exist
PageTable *vmm_get_pml4(uintptr_t virt_addr);
#endif

// Get the physical address for a given virtual address
// Returns 0 if the virtual address is not mapped
uintptr_t vmm_virt_to_phys(uintptr_t virt_addr);

// Initialize the direct mapping for physical memory
// This should be called during early boot
void vmm_init_direct_mapping(void);

// Map a physical page to a virtual address
// Returns true if successful, false otherwise
bool vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t flags);

// Unmap a virtual address
// Returns true if successful, false otherwise
bool vmm_unmap_page(uintptr_t virt_addr);

// Allocate a new page table
// Returns NULL if allocation fails
PageTable *vmm_alloc_page_table(void);

// Free a page table
void vmm_free_page_table(PageTable *pt);

#endif // __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H