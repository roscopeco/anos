/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "kdrivers/cpu.h"

#include "pmm/pagealloc.h"
#include "vmm/recursive.h"
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

#define SPIN_LOCK()                                                            \
    do {                                                                       \
        spinlock_lock(&vmm_map_lock);                                          \
    } while (0)

#define SPIN_UNLOCK_RET(retval)                                                \
    do {                                                                       \
        spinlock_unlock(&vmm_map_lock);                                        \
        return (retval);                                                       \
    } while (0)

#define NULL (((void *)0))

extern MemoryRegion *physical_region;
static SpinLock vmm_map_lock;

// TODO locking in here is very coarse-grained - it could be done based
//      on the top-level table instead, for example...
//

static inline uint64_t *ensure_table_entry(uint64_t *table, uint16_t index,
                                           uint16_t flags) {
    uint64_t entry = table[index];
    if ((entry & PRESENT) == 0) {
        uint64_t page = page_alloc(physical_region);

        if (page & 0xff) {
            // No page alloc - fail
            C_DEBUGSTR("===> FAIL to allocate new table\n");
            return NULL;
        }

#ifdef VERY_NOISY_VMM
        C_DEBUGSTR("===> New table at ");
        C_PRINTHEX64(pdpte_page, debugchar);
        C_DEBUGSTR("\n");
#endif

        uint64_t *page_v = PAGE_TO_V(page);
        for (int i = 0; i < 0x200; i++) {
            page_v[i] = 0;
        }
        table[index] = page | flags |
                       PRESENT; // Force present since we allocated a page...
    } else {
        // Table already mapped, but flags might not be correct, so let's merge
        // TODO I'm not certain this is a good idea but it'll work for now...
        table[index] = entry | flags | PRESENT;
    }

    return ENTRY_TO_V(table[index]);
}

inline bool vmm_map_page_in(uint64_t *pml4, uintptr_t virt_addr, uint64_t page,
                            uint16_t flags) {

    SPIN_LOCK();

    C_DEBUGSTR("PML4 @ ");
    C_PRINTHEX64((uint64_t)pml4, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PML4ENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PML4 entry at this index - create one, and a PDPT for
    // it to point to
    uint64_t *pdpt = ensure_table_entry(pml4, PML4ENTRY(virt_addr), flags);
    if (pdpt == NULL) {
        C_DEBUGSTR("===> vmm_map_page failed [PDPT alloc] for\n");
        C_PRINTHEX64(virt_addr, debugchar);
        C_DEBUGSTR(" => ");
        C_PRINTHEX64(page, debugchar);
        C_DEBUGSTR("\n");

        SPIN_UNLOCK_RET(false);
    }

    C_DEBUGSTR("PDPT @ ");
    C_PRINTHEX64((uint64_t)pdpt, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDPTENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PDPT entry at this index - create one, and a PD for it
    // to point to
    uint64_t *pd = ensure_table_entry(pdpt, PDPTENTRY(virt_addr), flags);
    if (pd == NULL) {
        C_DEBUGSTR("===> vmm_map_page failed [PD alloc] for\n");
        C_PRINTHEX64(virt_addr, debugchar);
        C_DEBUGSTR(" => ");
        C_PRINTHEX64(page, debugchar);
        C_DEBUGSTR("\n");

        SPIN_UNLOCK_RET(false);
    }

    C_DEBUGSTR("PD   @ ");
    C_PRINTHEX64((uint64_t)pd, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    // If we don't have a PD entry at this index - create one, and a PT for it
    // to point to
    uint64_t *pt = ensure_table_entry(pd, PDENTRY(virt_addr), flags);
    if (pt == NULL) {
        C_DEBUGSTR("===> vmm_map_page failed [PT alloc] for\n");
        C_PRINTHEX64(virt_addr, debugchar);
        C_DEBUGSTR(" => ");
        C_PRINTHEX64(page, debugchar);
        C_DEBUGSTR("\n");

        SPIN_UNLOCK_RET(false);
    }

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
    vmm_invalidate_page(virt_addr);

    SPIN_UNLOCK_RET(true);
}

bool vmm_map_page(uintptr_t virt_addr, uint64_t page, uint16_t flags) {
    return vmm_map_page_in((uint64_t *)vmm_recursive_find_pml4(), virt_addr,
                           page, flags);
}

bool vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr,
                             uint16_t flags) {
    return vmm_map_page(virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t virt_addr,
                                uint64_t phys_addr, uint16_t flags) {
    return vmm_map_page_in(pml4, virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr) {
    SPIN_LOCK();

    C_DEBUGSTR("Unmap virtual ");
    C_PRINTHEX64((uint64_t)virt_addr, debugchar);
    C_DEBUGSTR("\nPML4 @ ");
    C_PRINTHEX64((uint64_t)pml4, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PML4ENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    uintptr_t pdpt = (uintptr_t)PAGE_TO_V(pml4[PML4ENTRY(virt_addr)]);

    C_DEBUGSTR("PDPT @ ");
    C_PRINTHEX64((uintptr_t)ENTRY_TO_V(pdpt), debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDPTENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    if ((pdpt & PRESENT) == 0) {
        // No present PDPT, just bail
        C_DEBUGSTR("No PDPT (");
        C_PRINTHEX64(pdpt, debugchar);
        C_DEBUGSTR(") - Bailing\n");

        SPIN_UNLOCK_RET(0);
    }

    uintptr_t pd = (uintptr_t)PAGE_TO_V(ENTRY_TO_V(pdpt)[PDPTENTRY(virt_addr)]);

    C_DEBUGSTR("PD   @ ");
    C_PRINTHEX64((uintptr_t)ENTRY_TO_V(pd), debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PDENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    if ((pd & PRESENT) == 0) {
        // No present PD, just bail
        C_DEBUGSTR("No PD (");
        C_PRINTHEX64(pd, debugchar);
        C_DEBUGSTR(") - Bailing\n");

        SPIN_UNLOCK_RET(0);
    }

    uintptr_t pt = (uintptr_t)PAGE_TO_V(ENTRY_TO_V(pd)[PDENTRY(virt_addr)]);

    C_DEBUGSTR("PT   @ ");
    C_PRINTHEX64((uintptr_t)ENTRY_TO_V(pt), debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PTENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    if ((pt & PRESENT) == 0) {
        // No present PT, just bail
        C_DEBUGSTR("No PT (");
        C_PRINTHEX64(pt, debugchar);
        C_DEBUGSTR(") - Bailing\n");

        SPIN_UNLOCK_RET(0);
    }

    uintptr_t phys = (ENTRY_TO_V(pt)[PTENTRY(virt_addr)] & PAGE_ALIGN_MASK);

#ifdef VERY_NOISY_VMM
    C_DEBUGSTR("zeroing entry: ");
    C_PRINTHEX64(PTENTRY(virt_addr), debugchar);
    C_DEBUGSTR(" @ ");
    C_PRINTHEX64((uintptr_t)&ENTRY_TO_V(pt)[PTENTRY(virt_addr)], debugchar);
    C_DEBUGSTR(" (== ");
    C_PRINTHEX64((uintptr_t)ENTRY_TO_V(pt)[PTENTRY(virt_addr)], debugchar);
    C_DEBUGSTR(")\n");
#endif

    ENTRY_TO_V(pt)[PTENTRY(virt_addr)] = 0;
    vmm_invalidate_page(virt_addr);

    SPIN_UNLOCK_RET(phys);
}

uintptr_t vmm_unmap_page(uintptr_t virt_addr) {
    return vmm_unmap_page_in((uint64_t *)vmm_recursive_find_pml4(), virt_addr);
}

void vmm_invalidate_page(uintptr_t virt_addr) {
#ifndef UNIT_TESTS
#ifdef VERY_NOISY_VMM
    C_DEBUGSTR("INVALIDATE PAGE ");
    C_PRINTHEX64(virt_addr, debugchar);
    C_DEBUGSTR("\n");
#endif
    cpu_invalidate_page(virt_addr);
#endif
}
