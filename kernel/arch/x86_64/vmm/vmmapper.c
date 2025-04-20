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
#else
#include "x86_64/kdrivers/cpu.h"
#endif

#include "debugprint.h"
#include "printhex.h"
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
    V_DEBUGSTR("!! CLEAR TABLE @ ");
    V_PRINTHEX64(table, debugchar);
    V_DEBUGSTR("\n");

    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
}

// TODO this has become a bit of a mess lately - because of additional address
//      spaces at the other recursive mapping, we're having to go with direct
//      calls to vmm_recursive_table_address, and also relying on any second
//      address spaces also having their own temporary mapping in the other
//      spot (257).
//
//      We should move some of the logic back to recursive.h and tidy it up
//      so this isn't so tightly coupled.
//
static uint64_t *ensure_tables(uint16_t recursive_entry, uintptr_t virt_addr,
                               bool is_user) {
    // TODO this shouldn't leave the new tables as WRITE,
    // and also needs to handle the case where they exist but
    // are not WRITE....

    uint16_t pml4n = vmm_virt_to_pml4_index(virt_addr);
    uint64_t *pml4e = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, recursive_entry, recursive_entry,
            pml4n << 3);

    C_DEBUGSTR("MAP @ ");
    C_PRINTHEX64(recursive_entry, debugchar);
    C_DEBUGSTR("[");
    C_PRINTHEX64(pml4n, debugchar);
    C_DEBUGSTR("] = ");
    C_PRINTHEX64((uintptr_t)pml4e, debugchar);
    C_DEBUGSTR("\n");

    C_DEBUGSTR("  pml4e @ ");
    C_PRINTHEX64((uintptr_t)pml4e, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pml4e, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pml4e & PG_PRESENT) == 0) {
        // not present, needs mapping from pdpt down
        V_DEBUGSTR("    !! PML4E not present (no PDPT) - will allocate ...\n");

        uint64_t new_pdpt = page_alloc(physical_region) | PG_PRESENT |
                            PG_WRITE | (is_user ? PG_USER : 0);

        if (!new_pdpt) {
            C_DEBUGSTR("WARN: Failed to allocate page directory pointer table");
            return NULL;
        }

        // Map the page
        C_DEBUGSTR("        map new PDPT ");
        C_PRINTHEX64((uintptr_t)new_pdpt, debugchar);
        C_DEBUGSTR(" into PML4E @ ");
        C_PRINTHEX64((uintptr_t)pml4e, debugchar);
        C_DEBUGSTR(" old = [");
        C_PRINTHEX64(*pml4e, debugchar);
        C_DEBUGSTR("]\n");
        *pml4e = new_pdpt;

        uint64_t *base = (uint64_t *)vmm_recursive_table_address(
                recursive_entry, recursive_entry, recursive_entry, pml4n, 0);
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    } else {
        V_DEBUGSTR("    !! PML4E IS present (-> PDPT)\n");
    }

    uint16_t pdptn = vmm_virt_to_pdpt_index(virt_addr);
    uint64_t *pdpte = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, recursive_entry, pml4n,
            pdptn << 3);

    C_DEBUGSTR("  pdpte @ ");
    C_PRINTHEX64((uintptr_t)pdpte, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pdpte, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pdpte & PG_PRESENT) == 0) {
        // not present, needs mapping from pd down
        V_DEBUGSTR("    !! PDPTE not present (no PD) - will allocate ...\n");

        uint64_t new_pd = page_alloc(physical_region) | PG_PRESENT | PG_WRITE |
                          (is_user ? PG_USER : 0);

        if (!new_pd) {
            C_DEBUGSTR("WARN: Failed to allocate page directory");
            return NULL;
        }

        // Map the page
        C_DEBUGSTR("        map new PD ");
        C_PRINTHEX64((uintptr_t)new_pd, debugchar);
        C_DEBUGSTR(" into PDPTE @ ");
        C_PRINTHEX64((uintptr_t)pdpte, debugchar);
        C_DEBUGSTR(" old = [");
        C_PRINTHEX64(*pdpte, debugchar);
        C_DEBUGSTR("]\n");

        *pdpte = new_pd;

        uint64_t *base = (uint64_t *)vmm_recursive_table_address(
                recursive_entry, recursive_entry, pml4n, pdptn, 0);
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    } else {
        V_DEBUGSTR("    !! PDPTE IS present (-> PD)\n");
    }

    uint16_t pdn = vmm_virt_to_pd_index(virt_addr);
    uint64_t *pde = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, pml4n, pdptn, pdn << 3);

    C_DEBUGSTR("    pde @ ");
    C_PRINTHEX64((uintptr_t)pde, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pde, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pde & PG_PRESENT) == 0) {
        // not present, needs mapping from pt
        V_DEBUGSTR("    !! PDE not present (no PT) - will allocate ...\n");

        uint64_t new_pt = page_alloc(physical_region) | PG_PRESENT | PG_WRITE |
                          (is_user ? PG_USER : 0);

        if (!new_pt) {
            C_DEBUGSTR("WARN: Failed to allocate page table");
            return NULL;
        }

        // Map the page
        C_DEBUGSTR("        map new PT ");
        C_PRINTHEX64((uintptr_t)new_pt, debugchar);
        C_DEBUGSTR(" into PDE @ ");
        C_PRINTHEX64((uintptr_t)pde, debugchar);
        C_DEBUGSTR(" old = [");
        C_PRINTHEX64(*pde, debugchar);
        C_DEBUGSTR("]\n");

        *pde = new_pt;

        uint64_t *base = (uint64_t *)vmm_recursive_table_address(
                recursive_entry, pml4n, pdptn, pdn, 0);
        cpu_invalidate_page((uint64_t)base);

        clear_table(base);

        V_DEBUGSTR(" mapped\n");
    } else {
        V_DEBUGSTR("    !! PDE IS present (-> PT)\n");
    }

    uint16_t ptn = vmm_virt_to_pt_index(virt_addr);
    uint64_t *pte = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, pml4n, pdptn, pdn, ptn << 3);

    C_DEBUGSTR("    pte @ ");
    C_PRINTHEX64((uintptr_t)pde, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pte, debugchar);
    C_DEBUGSTR("]\n");

    return pte;
}

inline bool vmm_map_page_in(void *pml4, uintptr_t virt_addr, uint64_t page,
                            uint16_t flags) {

    uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);

    C_DEBUGSTR("==> MAP: ");
    C_PRINTHEX64(virt_addr, debugchar);
    C_DEBUGSTR(" = ");
    C_PRINTHEX64(page, debugchar);
    C_DEBUGSTR("\n");

    // This finds the PML4 entry that the PML4 we were given is referring to.
    // it's kind of a mess, but gives us the recursive entry in use.
    //
    uint16_t recursive_entry = vmm_recursive_pml4_virt_to_recursive_entry(pml4);
    uint64_t *pte = ensure_tables(recursive_entry, virt_addr, flags);

    V_DEBUGSTR("==> Ensured PTE @ ");
    V_PRINTHEX64((uintptr_t)pte, debugchar);
    V_DEBUGSTR("\n");
    *pte = page | flags;
    vmm_invalidate_page(virt_addr);

    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return true;
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

// Don't forget this requires a proper recursive mapping at whichever
// spot we're using as recursive in the given PML4!
//
uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr) {
    uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);

    C_DEBUGSTR("Unmap virtual ");
    C_PRINTHEX64((uint64_t)virt_addr, debugchar);
    C_DEBUGSTR("\nPML4 @ ");
    C_PRINTHEX64((uint64_t)pml4, debugchar);
    C_DEBUGSTR(" [Entry ");
    C_PRINTHEX64(PML4ENTRY(virt_addr), debugchar);
    C_DEBUGSTR("]\n");

    uint16_t recursive_entry = vmm_recursive_pml4_virt_to_recursive_entry(pml4);
    uint16_t pml4n = vmm_virt_to_pml4_index(virt_addr);
    uint64_t *pml4e = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, recursive_entry, recursive_entry,
            pml4n << 3);

    C_DEBUGSTR("  pdpte @ ");
    C_PRINTHEX64((uintptr_t)pml4e, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pml4e, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pml4e & PG_PRESENT) == 0) {
        // Not present PDPT, just bail
        C_DEBUGSTR("    No PDPT - Bailing\n");
        spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
        return 0;
    }

    uint16_t pdptn = vmm_virt_to_pdpt_index(virt_addr);
    uint64_t *pdpte = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, recursive_entry, pml4n,
            pdptn << 3);

    C_DEBUGSTR("  pdpte @ ");
    C_PRINTHEX64((uintptr_t)pdpte, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pdpte, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pdpte & PG_PRESENT) == 0) {
        // Not present PDPT, just bail
        C_DEBUGSTR("    No PD - Bailing\n");
        spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
        return 0;
    }

    uint16_t pdn = vmm_virt_to_pd_index(virt_addr);
    uint64_t *pde = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, recursive_entry, pml4n, pdptn, pdn << 3);

    C_DEBUGSTR("    pde @ ");
    C_PRINTHEX64((uintptr_t)pde, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pde, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pde & PG_PRESENT) == 0) {
        // Not present PDPT, just bail
        C_DEBUGSTR("    No PT - Bailing\n");
        spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
        return 0;
    }

    uint16_t ptn = vmm_virt_to_pt_index(virt_addr);
    uint64_t *pte = (uint64_t *)vmm_recursive_table_address(
            recursive_entry, pml4n, pdptn, pdn, ptn << 3);

    C_DEBUGSTR("     pt @ ");
    C_PRINTHEX64((uintptr_t)pte, debugchar);
    C_DEBUGSTR(" = [");
    C_PRINTHEX64(*pte, debugchar);
    C_DEBUGSTR("]\n");

    if ((*pte & PG_PRESENT) == 0) {
        // Not present PDPT, just bail
        C_DEBUGSTR("    No PT - Bailing\n");
        spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
        return 0;
    }

    uintptr_t phys = *pte & PAGE_ALIGN_MASK;

#ifdef VERY_NOISY_VMM
    C_DEBUGSTR("zeroing entry: ");
    C_PRINTHEX64(PTENTRY(virt_addr), debugchar);
    C_DEBUGSTR(" @ ");
    C_PRINTHEX64((uintptr_t)pte, debugchar);
    C_DEBUGSTR(" (== ");
    C_PRINTHEX64((uintptr_t)*pte, debugchar);
    C_DEBUGSTR(")\n");
#endif

    *pte = 0;
    vmm_invalidate_page(virt_addr);

    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return phys;
}

uintptr_t vmm_unmap_page(uintptr_t virt_addr) {
    return vmm_unmap_page_in((uint64_t *)vmm_recursive_find_pml4(), virt_addr);
}
