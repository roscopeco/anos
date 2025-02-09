/*
 * stage3 - Process address space handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef UNIT_TESTS
#include "mock_cpu.h"
#include "mock_recursive.h"
#else
#include "kdrivers/cpu.h"
#include "vmm/recursive.h"
#endif

#include "pmm/pagealloc.h"
#include "vmm/vmmapper.h"

extern MemoryRegion *physical_region;

bool address_space_init(void) {
    PageTable *pml4 = vmm_recursive_find_pml4();

    for (int i = RECURSIVE_ENTRY + 2; i < 512; i++) {
        if ((pml4->entries[i] & PRESENT) == 0) {

            // Allocate a page for this PDPT
            uintptr_t new_pdpt = page_alloc(physical_region);
            if (new_pdpt & 0xff) {
                // failed
                return false;
            }

            // Set up the new PDPT
            pml4->entries[i] = new_pdpt | WRITE | PRESENT;

            // Get a vaddr for this new table and invalidate TLB (just in case)
            uint64_t *vaddr = (uint64_t *)vmm_recursive_find_pdpt(i);
            cpu_invalidate_page((uintptr_t)vaddr);

            // Zero out the new table
            for (int i = 0; i < 512; i++) {
                vaddr[i] = 0;
            }
        }
    }

    return true;
}

uintptr_t create_address_space(void) { return 0; }