/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "pmm/pagealloc.h"
#include "vmm/vmmapper.h"

#ifdef UNIT_TESTS
#include "mock_cpu.h"
#include "mock_recursive.h"
#else
#include "kdrivers/cpu.h"
#include "vmm/recursive.h"
#endif

#ifdef DEBUG_VMM
#include "debugprint.h"
#include "printhex.h"

#define C_DEBUGSTR debugstr
#define C_PRINTHEX64 printhex64
#ifdef VERY_NOISY_VMM
#define V_DEBUGSTR debugstr
#define V_PRINTHEX64 printhex64
#else
#define V_DEBUGSTR(...)
#define V_PRINTHEX64(...)
#endif
#else
#define C_DEBUGSTR(...)
#define C_PRINTHEX64(...)
#define V_DEBUGSTR(...)
#define V_PRINTHEX64(...)
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

#ifndef NULL
#define NULL (((void *)0))
#endif

extern MemoryRegion *physical_region;
static SpinLock vmm_map_lock;

// TODO locking in here is very coarse-grained - it could be done based
//      on the top-level table instead, for example...
//

inline void vmm_invalidate_page(uintptr_t virt_addr) {
#ifndef UNIT_TESTS
#ifdef VERY_NOISY_VMM
    C_DEBUGSTR("INVALIDATE PAGE ");
    C_PRINTHEX64(virt_addr, debugchar);
    C_DEBUGSTR("\n");
#endif
    cpu_invalidate_page(virt_addr);
#endif
}

static inline void clear_table(uint64_t *table) {
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
}

static uint64_t *ensure_tables(uintptr_t pml4, uintptr_t virt_addr,
                               bool is_user) {
    // TODO this shouldn't leave the new tables as WRITE,
    // and also needs to handle the case where they exist but
    // are not WRITE....

    C_DEBUGSTR("   pml4 @ ");
    C_PRINTHEX64((uintptr_t)pml4, debugchar);
    C_DEBUGSTR("\n");

    uint64_t *pml4e = vmm_virt_to_pml4e(virt_addr);

    C_DEBUGSTR("  pml4e @ ");
    C_PRINTHEX64((uintptr_t)pml4e, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pml4e, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pml4e & PRESENT) == 0) {
        // not present, needs mapping from pdpt down
        V_DEBUGSTR("    !! Page not present (PML4E) ...");

        uint64_t new_pdpt = page_alloc(physical_region) | PRESENT | WRITE |
                            (is_user ? USER : 0);

        if (!new_pdpt) {
            C_DEBUGSTR("WARN: Failed to allocate page directory pointer table");
            return NULL;
        }

        // Map the page
        *pml4e = new_pdpt;
        uint64_t *base = (uint64_t *)((uint64_t)vmm_virt_to_pdpt(virt_addr));
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    }

    uint64_t *pdpte = vmm_virt_to_pdpte(virt_addr);

    C_DEBUGSTR("  pdpte @ ");
    C_PRINTHEX64((uintptr_t)pdpte, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pdpte, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pdpte & PRESENT) == 0) {
        // not present, needs mapping from pd down
        V_DEBUGSTR("    !! Page not present (PDPTE) ...");

        uint64_t new_pd = page_alloc(physical_region) | PRESENT | WRITE |
                          (is_user ? USER : 0);

        if (!new_pd) {
            C_DEBUGSTR("WARN: Failed to allocate page directory");
            return NULL;
        }

        // Map the page
        *pdpte = new_pd;
        uint64_t *base = (uint64_t *)((uint64_t)vmm_virt_to_pd(virt_addr));
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    }

    uint64_t *pde = vmm_virt_to_pde(virt_addr);

    C_DEBUGSTR("    pde @ ");
    C_PRINTHEX64((uintptr_t)pde, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pde, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pde & PRESENT) == 0) {
        // not present, needs mapping from pt
        V_DEBUGSTR("    !! Page not present (PDE) ...");

        uint64_t new_pt = page_alloc(physical_region) | PRESENT | WRITE |
                          (is_user ? USER : 0);

        if (!new_pt) {
            C_DEBUGSTR("WARN: Failed to allocate page table");
            return NULL;
        }

        // Map the page
        *pde = new_pt;
        uint64_t *base = (uint64_t *)((uint64_t)vmm_virt_to_pt(virt_addr));
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    }

    uint64_t *pte = vmm_virt_to_pte(virt_addr);

    C_DEBUGSTR("    pte @ ");
    C_PRINTHEX64((uintptr_t)pde, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pte, debugchar);
    C_DEBUGSTR("]\n");

    return pte;
}

inline bool vmm_map_page_in(void *pml4, uintptr_t virt_addr, uint64_t page,
                            uint16_t flags) {

    SPIN_LOCK();

    C_DEBUGSTR("==> MAP: ");
    C_PRINTHEX64(virt_addr, debugchar);
    C_DEBUGSTR(" = ");
    C_PRINTHEX64(page, debugchar);
    C_DEBUGSTR("\n");

    uint64_t *pte = ensure_tables((uintptr_t)pml4, virt_addr, flags);
    *pte = page | flags;
    vmm_invalidate_page(virt_addr);

    SPIN_UNLOCK_RET(true);
}

bool vmm_map_page(uintptr_t virt_addr, uint64_t page, uint16_t flags) {
    return vmm_map_page_in(vmm_recursive_find_pml4(), virt_addr, page, flags);
}

bool vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr,
                             uint16_t flags) {
    return vmm_map_page(virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t virt_addr,
                                uint64_t phys_addr, uint16_t flags) {
    return vmm_map_page_in(pml4, virt_addr, phys_addr & PAGE_ALIGN_MASK, flags);
}

// TODO this should be reworked to fit in with the new map_page_in implementation...
//
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
