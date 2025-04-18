/*
 * RISC-V Memory Management Example
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <arch/riscv/riscv_asm.h>
#include <pmm/pmm.h>
#include <stdio.h>
#include <vmm/riscv_mm.h>

// Example function to demonstrate how to use the direct mapping approach
void riscv_mm_example(void) {
    // Initialize the direct mapping for physical memory
    vmm_init_direct_mapping();

    // Now we can access physical memory directly through the direct mapping
    // For example, to access a physical address 0x1000:
    uintptr_t phys_addr = 0x1000;
    uintptr_t virt_addr = phys_to_virt(phys_addr);

    // We can now read/write to this address
    uint8_t *ptr = (uint8_t *)virt_addr;
    uint8_t value = *ptr;
    *ptr = 0x42;

    // We can also map a new page to a virtual address
    uintptr_t new_phys_addr = pmm_alloc_page();
    uintptr_t new_virt_addr = 0x20000000; // Some virtual address

    vmm_map_page(new_virt_addr, new_phys_addr, PG_PRESENT | PG_READ | PG_WRITE);

    // Now we can access this page
    uint8_t *new_ptr = (uint8_t *)new_virt_addr;
    *new_ptr = 0x42;

    // When we're done, we can unmap the page
    vmm_unmap_page(new_virt_addr);

    // And free the physical page
    pmm_free_page(new_phys_addr);

    // We can also convert between virtual and physical addresses
    uintptr_t some_virt_addr = 0x30000000;
    uintptr_t some_phys_addr = vmm_virt_to_phys(some_virt_addr);

    if (some_phys_addr) {
        printf("Virtual address 0x%lx maps to physical address 0x%lx\n",
               some_virt_addr, some_phys_addr);
    } else {
        printf("Virtual address 0x%lx is not mapped\n", some_virt_addr);
    }

    // We can also check if an address is in the direct mapping region
    if (is_direct_mapped(DIRECT_MAP_BASE + 0x1000)) {
        printf("Address 0x%lx is in the direct mapping region\n",
               DIRECT_MAP_BASE + 0x1000);
    }

    // And convert it back to a physical address
    uintptr_t direct_virt_addr = DIRECT_MAP_BASE + 0x1000;
    uintptr_t direct_phys_addr = virt_to_phys(direct_virt_addr);

    printf("Direct mapped virtual address 0x%lx corresponds to physical "
           "address 0x%lx\n",
           direct_virt_addr, direct_phys_addr);
}

// Example function to demonstrate how to access page tables
void riscv_mm_page_table_example(void) {
    // Get the page table entry for a virtual address
    uintptr_t virt_addr = 0x40000000;
    uint64_t *pte = vmm_get_pte(virt_addr);

    if (pte) {
        printf("Page table entry for virtual address 0x%lx: 0x%lx\n", virt_addr,
               *pte);

        // We can modify the page table entry
        // For example, to make it read-only:
        *pte &= ~PG_WRITE;

        // And invalidate the TLB for this address
        invalidate_tlb_addr(virt_addr);
    } else {
        printf("No page table entry for virtual address 0x%lx\n", virt_addr);
    }

    // We can also get the page table for a virtual address
    PageTable *pt = vmm_get_pt(virt_addr);

    if (pt) {
        printf("Page table for virtual address 0x%lx is at 0x%lx\n", virt_addr,
               (uintptr_t)pt);

        // We can access the page table entries directly
        for (int i = 0; i < 512; i++) {
            if (pt->entries[i] & PG_PRESENT) {
                printf("Page table entry %d: 0x%lx\n", i, pt->entries[i]);
            }
        }
    } else {
        printf("No page table for virtual address 0x%lx\n", virt_addr);
    }
}