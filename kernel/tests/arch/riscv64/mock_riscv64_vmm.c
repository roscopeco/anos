/*
 * Mock implementation of the RISC-V VMM for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mock_vmm.h"

// Constants needed for VMM operations
#define PAGE_SIZE ((uintptr_t)(0x1000))            // 4KB
#define MEGA_PAGE_SIZE ((uintptr_t)(0x200000))     // 2MB
#define GIGA_PAGE_SIZE ((uintptr_t)(0x40000000))   // 1GB
#define TERA_PAGE_SIZE ((uintptr_t)(0x8000000000)) // 512GB
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))
#define DIRECT_MAP_BASE 0xffff800000000000ULL
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))

// PTE flags
#define PG_PRESENT (1ULL << 0)  // Valid
#define PG_READ (1ULL << 1)     // Read
#define PG_WRITE (1ULL << 2)    // Write
#define PG_EXEC (1ULL << 3)     // Execute
#define PG_USER (1ULL << 4)     // User
#define PG_GLOBAL (1ULL << 5)   // Global
#define PG_ACCESSED (1ULL << 6) // Accessed
#define PG_DIRTY (1ULL << 7)    // Dirty

// Mock state tracking
static uint32_t total_page_maps = 0;
static uint32_t total_page_unmaps = 0;

static uint64_t last_page_map_paddr = 0;
static uint64_t last_page_map_vaddr = 0;
static uint16_t last_page_map_flags = 0;
static uint64_t last_page_map_pml4 = 0;

static uintptr_t last_page_unmap_pml4 = 0;
static uintptr_t last_page_unmap_virt = 0;

// Define a simple paging structure for tracking mappings
typedef struct {
    uintptr_t virt_addr;
    uintptr_t phys_addr;
    uint16_t flags;
} MockPageMapping;

#define MAX_MOCK_MAPPINGS 1024
static MockPageMapping mock_mappings[MAX_MOCK_MAPPINGS];
static size_t mock_mappings_count = 0;

// Reset the mock state
void mock_vmm_reset() {
    total_page_maps = 0;
    total_page_unmaps = 0;
    mock_mappings_count = 0;
    memset(mock_mappings, 0, sizeof(mock_mappings));
}

// Getters for test verification
uint64_t mock_vmm_get_last_page_map_paddr() { return last_page_map_paddr; }
uint64_t mock_vmm_get_last_page_map_vaddr() { return last_page_map_vaddr; }
uint16_t mock_vmm_get_last_page_map_flags() { return last_page_map_flags; }
uint64_t mock_vmm_get_last_page_map_pml4() { return last_page_map_pml4; }
uint32_t mock_vmm_get_total_page_maps() { return total_page_maps; }
uint32_t mock_vmm_get_total_page_unmaps() { return total_page_unmaps; }
uintptr_t mock_vmm_get_last_page_unmap_pml4() { return last_page_unmap_pml4; }
uintptr_t mock_vmm_get_last_page_unmap_virt() { return last_page_unmap_virt; }

// Helper functions
static inline uintptr_t vmm_phys_to_virt(uintptr_t phys_addr) {
    return DIRECT_MAP_BASE + phys_addr;
}

static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) {
    return virt_addr - DIRECT_MAP_BASE;
}

static inline uint64_t vmm_satp_to_root_table_phys(uint64_t satp) {
    // In RISC-V, the SATP register contains the physical address of the root page table
    // This is a simplification for the mock
    return (satp & 0x00000FFFFFFFFFFF) << 12;
}

// Mock RISC-V CPU functions
uint64_t cpu_read_satp() {
    // Return a mock SATP value pointing to a valid root table
    return 0x8000000000000001; // Mode = Sv39 (8), ASID = 0, PPN = 1
}

uint64_t cpu_satp_to_root_table_phys(uint64_t satp) {
    return vmm_satp_to_root_table_phys(satp);
}

// VMM function implementations
bool vmm_map_page_in(void *pml4, uintptr_t virt_addr, uint64_t page,
                     uint16_t flags) {
    // Record for testing
    last_page_map_paddr = page;
    last_page_map_vaddr = virt_addr;
    last_page_map_flags = flags;
    last_page_map_pml4 = (uint64_t)pml4;

    // Add to mock mappings
    if (mock_mappings_count < MAX_MOCK_MAPPINGS) {
        mock_mappings[mock_mappings_count].virt_addr = virt_addr;
        mock_mappings[mock_mappings_count].phys_addr = page;
        mock_mappings[mock_mappings_count].flags = flags;
        mock_mappings_count++;
    }

    total_page_maps++;
    return true;
}

bool vmm_map_page(uintptr_t virt_addr, uint64_t page, uint16_t flags) {
    uint64_t satp = cpu_read_satp();
    void *pml4 = (void *)vmm_phys_to_virt(vmm_satp_to_root_table_phys(satp));
    return vmm_map_page_in(pml4, virt_addr, page, flags);
}

uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr) {
    last_page_unmap_pml4 = (uintptr_t)pml4;
    last_page_unmap_virt = virt_addr;

    // Find and remove from mock mappings
    uintptr_t phys_addr = 0;
    for (size_t i = 0; i < mock_mappings_count; i++) {
        if (mock_mappings[i].virt_addr == virt_addr) {
            phys_addr = mock_mappings[i].phys_addr;

            // Remove by shifting the array (simple approach)
            if (i < mock_mappings_count - 1) {
                memmove(&mock_mappings[i], &mock_mappings[i + 1],
                        (mock_mappings_count - i - 1) *
                                sizeof(MockPageMapping));
            }
            mock_mappings_count--;
            break;
        }
    }

    total_page_unmaps++;
    return phys_addr;
}

bool vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr,
                             uint16_t flags) {
    return vmm_map_page(virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

uintptr_t vmm_unmap_page(uintptr_t virt_addr) {
    uint64_t satp = cpu_read_satp();
    void *pml4 = (void *)vmm_phys_to_virt(vmm_satp_to_root_table_phys(satp));
    return vmm_unmap_page_in(pml4, virt_addr);
}

// Additional helper functions for RISC-V-specific VMM testing
bool mock_vmm_is_page_mapped(uintptr_t virt_addr) {
    for (size_t i = 0; i < mock_mappings_count; i++) {
        if (mock_mappings[i].virt_addr == (virt_addr & PAGE_ALIGN_MASK)) {
            return true;
        }
    }
    return false;
}

uintptr_t mock_vmm_get_phys_for_virt(uintptr_t virt_addr) {
    for (size_t i = 0; i < mock_mappings_count; i++) {
        if (mock_mappings[i].virt_addr == (virt_addr & PAGE_ALIGN_MASK)) {
            return mock_mappings[i].phys_addr;
        }
    }
    return 0; // Not mapped
}

uint16_t mock_vmm_get_flags_for_virt(uintptr_t virt_addr) {
    for (size_t i = 0; i < mock_mappings_count; i++) {
        if (mock_mappings[i].virt_addr == (virt_addr & PAGE_ALIGN_MASK)) {
            return mock_mappings[i].flags;
        }
    }
    return 0; // Not mapped
}

size_t mock_vmm_get_mapping_count() { return mock_mappings_count; }