/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "pmm/pagealloc.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_VMM
#include "debugprint.h"
#include "printhex.h"
#endif

#ifdef DEBUG_VMM
#define C_DEBUGSTR debugstr
#define C_PRINTHEX64 printhex64
#else
#define C_DEBUGSTR(...)
#define C_PRINTHEX64(...)
#endif

#ifdef UNIT_TESTS
#define PAGE_TO_V(page) ((uint64_t *)page)
#define ENTRY_TO_V(entry) ((uint64_t *)(entry & PAGE_ALIGN_MASK))
#else
#define PAGE_TO_V(page) ((uint64_t *)(page | STATIC_KERNEL_SPACE))
#define ENTRY_TO_V(entry)                                                      \
    ((uint64_t *)((entry | STATIC_KERNEL_SPACE) & PAGE_ALIGN_MASK))
#endif

extern MemoryRegion *physical_region;

static inline uint64_t *ensure_table_entry(uint64_t *table, uint16_t index,
                                           uint16_t flags) {
    uint64_t entry = table[index];
    if ((entry & PRESENT) == 0) {
        uint64_t page = page_alloc(physical_region);

#ifdef VERY_NOISY_VMM
        C_DEBUGSTR("===> New PD at ");
        C_PRINTHEX64(pdpte_page, debugchar);
        C_DEBUGSTR("\n");
#endif

        uint64_t *page_v = PAGE_TO_V(page);
        for (int i = 0; i < 0x200; i++) {
            page_v[i] = 0;
        }
        table[index] = page | flags;
    }

    return ENTRY_TO_V(table[index]);
}

void vmm_map_page(uint64_t *pml4, uintptr_t virt_addr, uint64_t page,
                  uint16_t flags) {
    C_DEBUGSTR("PML4 @ ");
    C_PRINTHEX64((uint64_t)pml4, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PML4ENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PML4 entry at this index - create one, and a PDPT for
    // it to point to
    uint64_t *pdpt = ensure_table_entry(pml4, PML4ENTRY(virt_addr), flags);

    C_DEBUGSTR("PDPT @ ");
    C_PRINTHEX64((uint64_t)pdpt, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDPTENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PDPT entry at this index - create one, and a PD for it
    // to point to
    uint64_t *pd = ensure_table_entry(pdpt, PDPTENTRY(virt_addr), flags);

    C_DEBUGSTR("PD   @ ");
    C_PRINTHEX64((uint64_t)pd, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PD entry at this index - create one, and a PT for it
    // to point to
    uint64_t *pt = ensure_table_entry(pd, PDENTRY(virt_addr), flags);

    C_DEBUGSTR("PT   @ ");
    C_PRINTHEX64((uint64_t)pt, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PTENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

#ifdef VERY_NOISY_VMM
    C_DEBUGSTR("Mapping PT entry ");
    C_PRINTHEX16(PTENTRY(virt_addr), debugchar);
    C_DEBUGSTR(" in page table at ");
    C_PRINTHEX64((uint64_t)pt, debugchar);
    C_DEBUGSTR(" to phys ");
    C_PRINTHEX64(page, debugchar);
    C_DEBUGSTR("\n");
#endif

    pt[PTENTRY(virt_addr)] = page | flags;

    return;
}

void vmm_map_page_containing(uint64_t *pml4, uintptr_t virt_addr,
                             uint64_t phys_addr, uint16_t flags) {
    vmm_map_page(pml4, virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}
