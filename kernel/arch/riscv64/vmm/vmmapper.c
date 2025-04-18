/*
 * RISC-V Memory Management
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stddef.h>
#include <stdint.h>

#include "kdrivers/cpu.h"
#include "pmm/pagealloc.h"
#include "std/string.h"
#include "vmm/vmmapper.h"

// Define SATP mode if not already defined
#ifndef SATP_MODE_SV39
#define SATP_MODE_SV39 8
#endif

// Current page table hierarchy
static PageTableHierarchy current_hierarchy;

// Physical memory region
extern MemoryRegion *physical_region;

// Initialize the direct mapping for physical memory
void vmm_init_direct_mapping(void) {
    // Allocate the top-level page table
    current_hierarchy.l2 = vmm_alloc_page_table();
    if (!current_hierarchy.l2) {
        // Handle error
        return;
    }

#ifdef RISCV_SV48
    // For Sv48, we need to allocate L3 as well
    current_hierarchy.l3 = vmm_alloc_page_table();
    if (!current_hierarchy.l3) {
        // Handle error
        return;
    }
#endif

    // Set up the direct mapping for physical memory
    // We'll use huge pages (2MB) for efficiency
    uintptr_t phys_addr = 0;
    uintptr_t virt_addr = DIRECT_MAP_BASE;

    // Map all physical memory using huge pages
    // This is a simplified version - in a real implementation,
    // you would get the actual physical memory size from the bootloader
    // or memory manager
    while (phys_addr < 0x100000000) { // 4GB for this example
        vmm_map_page(virt_addr, phys_addr,
                     PG_PRESENT | PG_READ | PG_WRITE | PG_GLOBAL);
        phys_addr += MEGA_PAGE_SIZE;
        virt_addr += MEGA_PAGE_SIZE;
    }

    // Set the page table base address in the SATP CSR
    uintptr_t pt_base = virt_to_phys((uintptr_t)current_hierarchy.l2);
#ifdef RISCV_SV48
    cpu_set_satp(pt_base, SATP_MODE_SV48);
#else
    cpu_set_satp(pt_base, SATP_MODE_SV39);
#endif
}

// Get the page table entry for a given virtual address
uint64_t *vmm_get_pte(uintptr_t virt_addr) {
    // Extract the page table indices from the virtual address
    uint16_t l0_index = (virt_addr >> 12) & 0x1FF;
    uint16_t l1_index = (virt_addr >> 21) & 0x1FF;
    uint16_t l2_index = (virt_addr >> 30) & 0x1FF;
#ifdef RISCV_SV48
    uint16_t l3_index = (virt_addr >> 39) & 0x1FF;
#endif

    // Get the physical address of the page table
    uintptr_t pt_phys_addr;

#ifdef RISCV_SV48
    // For Sv48, we need to traverse L3, L2, L1, L0
    if (!current_hierarchy.l3)
        return NULL;

    // Get L3 entry
    uint64_t l3_entry = current_hierarchy.l3->entries[l3_index];
    if (!(l3_entry & PG_PRESENT))
        return NULL;

    // Get L2
    PageTable *l2 = (PageTable *)phys_to_virt(l3_entry & ~0xFFF);
    if (!l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
    if (!l1)
        return NULL;

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT))
        return NULL;

    // Get L0
    PageTable *l0 = (PageTable *)phys_to_virt(l1_entry & ~0xFFF);
    if (!l0)
        return NULL;

    // Return L0 entry
    return &l0->entries[l0_index];
#else
    // For Sv39, we need to traverse L2, L1, L0
    if (!current_hierarchy.l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = current_hierarchy.l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
    if (!l1)
        return NULL;

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT))
        return NULL;

    // Get L0
    PageTable *l0 = (PageTable *)phys_to_virt(l1_entry & ~0xFFF);
    if (!l0)
        return NULL;

    // Return L0 entry
    return &l0->entries[l0_index];
#endif
}

// Get the page table for a given virtual address
PageTable *vmm_get_pt(uintptr_t virt_addr) {
    // Extract the page table indices from the virtual address
    uint16_t l1_index = (virt_addr >> 21) & 0x1FF;
    uint16_t l2_index = (virt_addr >> 30) & 0x1FF;
#ifdef RISCV_SV48
    uint16_t l3_index = (virt_addr >> 39) & 0x1FF;
#endif

#ifdef RISCV_SV48
    // For Sv48, we need to traverse L3, L2, L1
    if (!current_hierarchy.l3)
        return NULL;

    // Get L3 entry
    uint64_t l3_entry = current_hierarchy.l3->entries[l3_index];
    if (!(l3_entry & PG_PRESENT))
        return NULL;

    // Get L2
    PageTable *l2 = (PageTable *)phys_to_virt(l3_entry & ~0xFFF);
    if (!l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
    if (!l1)
        return NULL;

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT))
        return NULL;

    // Get L0
    return (PageTable *)phys_to_virt(l1_entry & ~0xFFF);
#else
    // For Sv39, we need to traverse L2, L1
    if (!current_hierarchy.l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = current_hierarchy.l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
    if (!l1)
        return NULL;

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT))
        return NULL;

    // Get L0
    return (PageTable *)phys_to_virt(l1_entry & ~0xFFF);
#endif
}

// Get the page directory for a given virtual address
PageTable *vmm_get_pd(uintptr_t virt_addr) {
    // Extract the page table indices from the virtual address
    uint16_t l2_index = (virt_addr >> 30) & 0x1FF;
#ifdef RISCV_SV48
    uint16_t l3_index = (virt_addr >> 39) & 0x1FF;
#endif

#ifdef RISCV_SV48
    // For Sv48, we need to traverse L3, L2
    if (!current_hierarchy.l3)
        return NULL;

    // Get L3 entry
    uint64_t l3_entry = current_hierarchy.l3->entries[l3_index];
    if (!(l3_entry & PG_PRESENT))
        return NULL;

    // Get L2
    PageTable *l2 = (PageTable *)phys_to_virt(l3_entry & ~0xFFF);
    if (!l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    return (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
#else
    // For Sv39, we need to traverse L2
    if (!current_hierarchy.l2)
        return NULL;

    // Get L2 entry
    uint64_t l2_entry = current_hierarchy.l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT))
        return NULL;

    // Get L1
    return (PageTable *)phys_to_virt(l2_entry & ~0xFFF);
#endif
}

// Get the page directory pointer table for a given virtual address
PageTable *vmm_get_pdpt(uintptr_t virt_addr) {
#ifdef RISCV_SV48
    // Extract the page table indices from the virtual address
    uint16_t l3_index = (virt_addr >> 39) & 0x1FF;

    // For Sv48, we need to traverse L3
    if (!current_hierarchy.l3)
        return NULL;

    // Get L3 entry
    uint64_t l3_entry = current_hierarchy.l3->entries[l3_index];
    if (!(l3_entry & PG_PRESENT))
        return NULL;

    // Get L2
    return (PageTable *)phys_to_virt(l3_entry & ~0xFFF);
#else
    // For Sv39, L2 is the top level
    return current_hierarchy.l2;
#endif
}

#ifdef RISCV_SV48
// Get the page map level 4 for a given virtual address
PageTable *vmm_get_pml4(uintptr_t virt_addr) {
    // For Sv48, L3 is the top level
    return current_hierarchy.l3;
}
#endif

// Get the physical address for a given virtual address
uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) {
    // Check if the address is in the direct mapping region
    if (is_direct_mapped(virt_addr)) {
        return virt_to_phys(virt_addr);
    }

    // Get the page table entry
    uint64_t *pte = vmm_get_pte(virt_addr);
    if (!pte || !(*pte & PG_PRESENT)) {
        return 0;
    }

    // Extract the physical address from the page table entry
    uintptr_t phys_addr = *pte & ~0xFFF;

    // Add the page offset
    phys_addr |= virt_addr & 0xFFF;

    return phys_addr;
}

// Map a physical page to a virtual address
bool vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t flags) {
    // Extract the page table indices from the virtual address
    uint16_t l0_index = (virt_addr >> 12) & 0x1FF;
    uint16_t l1_index = (virt_addr >> 21) & 0x1FF;
    uint16_t l2_index = (virt_addr >> 30) & 0x1FF;
#ifdef RISCV_SV48
    uint16_t l3_index = (virt_addr >> 39) & 0x1FF;
#endif

    // Ensure the page tables exist
#ifdef RISCV_SV48
    // For Sv48, we need to ensure L3, L2, L1, L0 exist
    if (!current_hierarchy.l3) {
        current_hierarchy.l3 = vmm_alloc_page_table();
        if (!current_hierarchy.l3)
            return false;
    }

    // Get L3 entry
    uint64_t l3_entry = current_hierarchy.l3->entries[l3_index];
    if (!(l3_entry & PG_PRESENT)) {
        // Allocate L2
        PageTable *l2 = vmm_alloc_page_table();
        if (!l2)
            return false;

        // Set L3 entry
        current_hierarchy.l3->entries[l3_index] =
                ((uintptr_t)virt_to_phys((uintptr_t)l2) & ~0xFFF) | PG_PRESENT;
    }

    // Get L2
    PageTable *l2 = (PageTable *)phys_to_virt(
            current_hierarchy.l3->entries[l3_index] & ~0xFFF);

    // Get L2 entry
    uint64_t l2_entry = l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT)) {
        // Allocate L1
        PageTable *l1 = vmm_alloc_page_table();
        if (!l1)
            return false;

        // Set L2 entry
        l2->entries[l2_index] =
                ((uintptr_t)virt_to_phys((uintptr_t)l1) & ~0xFFF) | PG_PRESENT;
    }

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(l2->entries[l2_index] & ~0xFFF);

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT)) {
        // Allocate L0
        PageTable *l0 = vmm_alloc_page_table();
        if (!l0)
            return false;

        // Set L1 entry
        l1->entries[l1_index] =
                ((uintptr_t)virt_to_phys((uintptr_t)l0) & ~0xFFF) | PG_PRESENT;
    }

    // Get L0
    PageTable *l0 = (PageTable *)phys_to_virt(l1->entries[l1_index] & ~0xFFF);

    // Set L0 entry
    l0->entries[l0_index] = (phys_addr & ~0xFFF) | flags;
#else
    // For Sv39, we need to ensure L2, L1, L0 exist
    if (!current_hierarchy.l2) {
        current_hierarchy.l2 = vmm_alloc_page_table();
        if (!current_hierarchy.l2)
            return false;
    }

    // Get L2 entry
    uint64_t l2_entry = current_hierarchy.l2->entries[l2_index];
    if (!(l2_entry & PG_PRESENT)) {
        // Allocate L1
        PageTable *l1 = vmm_alloc_page_table();
        if (!l1)
            return false;

        // Set L2 entry
        current_hierarchy.l2->entries[l2_index] =
                ((uintptr_t)virt_to_phys((uintptr_t)l1) & ~0xFFF) | PG_PRESENT;
    }

    // Get L1
    PageTable *l1 = (PageTable *)phys_to_virt(
            current_hierarchy.l2->entries[l2_index] & ~0xFFF);

    // Get L1 entry
    uint64_t l1_entry = l1->entries[l1_index];
    if (!(l1_entry & PG_PRESENT)) {
        // Allocate L0
        PageTable *l0 = vmm_alloc_page_table();
        if (!l0)
            return false;

        // Set L1 entry
        l1->entries[l1_index] =
                ((uintptr_t)virt_to_phys((uintptr_t)l0) & ~0xFFF) | PG_PRESENT;
    }

    // Get L0
    PageTable *l0 = (PageTable *)phys_to_virt(l1->entries[l1_index] & ~0xFFF);

    // Set L0 entry
    l0->entries[l0_index] = (phys_addr & ~0xFFF) | flags;
#endif

    // Invalidate the TLB for this address
    cpu_invalidate_tlb_addr(virt_addr);

    return true;
}

// Unmap a virtual address
bool vmm_unmap_page(uintptr_t virt_addr) {
    // Get the page table entry
    uint64_t *pte = vmm_get_pte(virt_addr);
    if (!pte || !(*pte & PG_PRESENT)) {
        return false;
    }

    // Clear the page table entry
    *pte = 0;

    // Invalidate the TLB for this address
    cpu_invalidate_tlb_addr(virt_addr);

    return true;
}

// Allocate a new page table
PageTable *vmm_alloc_page_table(void) {
    // Allocate a physical page for the page table
    uintptr_t phys_addr = page_alloc(physical_region);
    if (!phys_addr) {
        return NULL;
    }

    // Convert to virtual address
    PageTable *pt = (PageTable *)phys_to_virt(phys_addr);

    // Clear the page table
    memset(pt, 0, sizeof(PageTable));

    return pt;
}

// Free a page table
void vmm_free_page_table(PageTable *pt) {
    if (!pt) {
        return;
    }

    // Convert to physical address
    uintptr_t phys_addr = virt_to_phys((uintptr_t)pt);

    // Free the physical page
    page_free(physical_region, phys_addr);
}