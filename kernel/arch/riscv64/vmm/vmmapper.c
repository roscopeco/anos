/*
 * RISC-V virtual memory manager
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * Easy-peasy, based on the direct mapping. The hard work to set
 * that up happens in vmmapper_init.c :)
 */

#include <stddef.h>
#include <stdint.h>

#include "panic.h"
#include "pmm/pagealloc.h"
#include "riscv64/kdrivers/cpu.h"
#include "riscv64/vmm/vmconfig.h"
#include "spinlock.h"
#include "std/string.h"

#include "vmm/vmmapper.h"

#ifdef DEBUG_VMM
#include "kprintf.h"
#define debugf printf
#ifdef VERY_NOISY_VMM
#define vdebugf printf
#else
#define vdebugf(...)
#endif
#else
#define debugf(...)
#define vdebugf(...)
#endif

#ifdef UNIT_TESTS
#define STATIC_EXCEPT_TESTS
#else
#define STATIC_EXCEPT_TESTS static
#endif

extern MemoryRegion *physical_region;

static SpinLock vmm_map_lock;

// TODO locking in here is very coarse-grained - it could be done based
//      on the top-level table instead, for example...
//

/*
 * Returns true if the given entry is a leaf, i.e.
 * has any of the READ, WRITE or EXEC bits set.
 */
STATIC_EXCEPT_TESTS inline bool is_leaf(uint64_t table_entry) {
    return table_entry & (PG_READ | PG_WRITE | PG_EXEC);
}

/*
 * Ensure tables are mapped to the specified level.
 *
 * levels (1-based to fit with PML4 naming):
 *
 *   4 - Ensures PML4
 *   3 - Ensures PDPT
 *   2 - Ensures PD
 *   1 - Ensures PT
 * 
 * Returns a virtual pointer to the table of the specified 
 * level, which may be newly created.
 * 
 * Must be called with VMM locked!
 */
STATIC_EXCEPT_TESTS uint64_t *ensure_tables(const uint64_t *root_table,
                                            const uintptr_t virt_addr,
                                            const PagetableLevel to_level) {
    if (to_level < 1 || to_level > 4) {
        return NULL;
    }

    int levels_remain = 4 - to_level;
    uint64_t *current_table = (uint64_t *)root_table;

    while (levels_remain) {
        const uint16_t current_index =
                vmm_virt_to_table_index(virt_addr, levels_remain + 1);
        uint64_t current_entry = current_table[current_index];

        if ((current_entry & PG_PRESENT) == 0) {
            uintptr_t new_table = page_alloc(physical_region);

            if (new_table & 0xfff) {
                // TODO unmap what we mapped so far? Is it worth it?
                return NULL;
            }

            uint64_t *new_ptr = vmm_phys_to_virt_ptr(new_table);

            memclr(new_ptr, VM_PAGE_SIZE);

            current_table[current_index] =
                    vmm_phys_and_flags_to_table_entry(new_table, PG_PRESENT);
            current_table = new_ptr;
        } else {
            // special case - if this is a leaf, but we wanted a lower level,
            // we'll just fail...
            if (levels_remain > 1 && is_leaf(current_entry)) {
                return NULL;
            }

            current_table = vmm_phys_to_virt_ptr(
                    vmm_table_entry_to_phys(current_entry));
        }

        levels_remain -= 1;
    }

    return current_table;
}

inline void vmm_invalidate_page(uintptr_t virt_addr) {
    cpu_invalidate_tlb_addr(virt_addr);
}

static bool nolock_vmm_map_page_containing_in(uint64_t *pml4,
                                              uintptr_t virt_addr,
                                              const uint64_t phys_addr,
                                              const uint16_t flags) {

    virt_addr &= PAGE_ALIGN_MASK;

    uint64_t *pt = ensure_tables(pml4, virt_addr, PT_LEVEL_PT);

    if (!pt) {
        return false;
    }

    pt[vmm_virt_to_pt_index(virt_addr)] =
            vmm_phys_and_flags_to_table_entry(phys_addr, flags);

    vmm_invalidate_page(virt_addr);

    return true;
}

inline bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t virt_addr,
                                       const uint64_t phys_addr,
                                       const uint16_t flags) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    const bool result = nolock_vmm_map_page_containing_in(pml4, virt_addr,
                                                          phys_addr, flags);
    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return result;
}

inline bool vmm_map_page_containing(const uintptr_t virt_addr,
                                    const uint64_t phys_addr,
                                    const uint16_t flags) {
    return vmm_map_page_containing_in(
            vmm_phys_to_virt_ptr(cpu_satp_to_root_table_phys(cpu_read_satp())),
            virt_addr, phys_addr, flags);
}

bool vmm_map_page_in(uint64_t *pml4, const uintptr_t virt_addr,
                     const uint64_t page, const uint16_t flags) {
    return vmm_map_page_containing_in(pml4, virt_addr, page, flags);
}

bool vmm_map_page(const uintptr_t virt_addr, const uint64_t page,
                  const uint16_t flags) {
    return vmm_map_page_containing(virt_addr, page, flags);
}

inline bool vmm_map_pages_containing_in(uint64_t *pml4,
                                        const uintptr_t virt_addr,
                                        const uint64_t phys_addr,
                                        const uint16_t flags,
                                        const size_t num_pages) {

    uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    for (int i = 0; i < num_pages; i++) {
        if (!nolock_vmm_map_page_containing_in(
                    pml4, virt_addr + (i << VM_PAGE_LINEAR_SHIFT),
                    phys_addr + (i << VM_PAGE_LINEAR_SHIFT), flags)) {
            return false;
        }
    }
    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return true;
}

inline bool vmm_map_pages_containing(const uintptr_t virt_addr,
                                     const uint64_t phys_addr,
                                     const uint16_t flags,
                                     const size_t num_pages) {

    return vmm_map_pages_containing_in(
            vmm_phys_to_virt_ptr(cpu_satp_to_root_table_phys(cpu_read_satp())),
            virt_addr, phys_addr, flags, num_pages);
}

bool vmm_map_pages_in(uint64_t *pml4, const uintptr_t virt_addr,
                      const uint64_t page, const uint16_t flags,
                      const size_t num_pages) {
    return vmm_map_pages_containing_in(pml4, virt_addr, page, flags, num_pages);
}

bool vmm_map_pages(const uintptr_t virt_addr, const uint64_t page,
                   const uint16_t flags, const size_t num_pages) {
    return vmm_map_pages_containing(virt_addr, page, flags, num_pages);
}

static uintptr_t nolock_vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr) {
    const uint16_t pml4_index = vmm_virt_to_pml4_index(virt_addr);
    uint64_t pml4e = pml4[pml4_index];

    if (pml4e & PG_PRESENT) {
        if (is_leaf(pml4e)) {
            // unmapping a terapage
            pml4[pml4_index] = 0;

            vmm_invalidate_page(virt_addr);

            return vmm_table_entry_to_phys(pml4e);
        }

        uint64_t *pdpt = vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pml4e));
        const uint16_t pdpt_index = vmm_virt_to_pdpt_index(virt_addr);
        uint64_t pdpte = pdpt[pdpt_index];

        if (pdpte & PG_PRESENT) {
            if (is_leaf(pdpte)) {
                // unmapping a gigapage
                pdpt[pdpt_index] = 0;

                vmm_invalidate_page(virt_addr);

                return vmm_table_entry_to_phys(pdpte);
            }

            uint64_t *pd = vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pdpte));
            uint16_t pd_index = vmm_virt_to_pd_index(virt_addr);
            uint64_t pde = pd[pd_index];

            if (pde & PG_PRESENT) {
                if (is_leaf(pde)) {
                    // unmapping a megapage
                    pd[pd_index] = 0;
                    vmm_invalidate_page(virt_addr);

                    return vmm_table_entry_to_phys(pde);
                }

                uint64_t *pt =
                        vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pde));
                uint16_t pt_index = vmm_virt_to_pt_index(virt_addr);
                uint64_t pte = pt[pt_index];

                if (pte & PG_PRESENT) {
                    // unmapping a page
                    pt[pt_index] = 0;

                    vmm_invalidate_page(virt_addr);

                    return vmm_table_entry_to_phys(pte);
                }
            }
        }
    }

    return 0;
}

inline uintptr_t vmm_unmap_pages_in(uint64_t *pml4, uintptr_t virt_addr,
                                    size_t num_pages) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    const uintptr_t result = nolock_vmm_unmap_page_in(pml4, virt_addr);

    for (int i = 1; i < num_pages; i++) {
        nolock_vmm_unmap_page_in(pml4, virt_addr + (i << VM_PAGE_LINEAR_SHIFT));
    }

    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return result;
}

inline uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    const uintptr_t result = nolock_vmm_unmap_page_in(pml4, virt_addr);
    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return result;
}

uintptr_t vmm_unmap_pages(const uintptr_t virt_addr, size_t num_pages) {
    return vmm_unmap_pages_in(
            vmm_phys_to_virt_ptr(cpu_satp_to_root_table_phys(cpu_read_satp())),
            virt_addr, num_pages);
}

uintptr_t vmm_unmap_page(const uintptr_t virt_addr) {
    return vmm_unmap_page_in(
            vmm_phys_to_virt_ptr(cpu_satp_to_root_table_phys(cpu_read_satp())),
            virt_addr);
}

uintptr_t vmm_get_pagetable_root_phys() {
    return cpu_satp_to_root_table_phys(cpu_read_satp());
}

uint64_t vmm_virt_to_pt_entry(const uintptr_t virt_addr) {
    const uint64_t pml4e =
            vmm_find_pml4()->entries[vmm_virt_to_pml4_index(virt_addr)];
    if (pml4e & PG_PRESENT) {
        const uint64_t pdpte =
                ((PageTable *)vmm_phys_to_virt(vmm_table_entry_to_phys(pml4e)))
                        ->entries[vmm_virt_to_pdpt_index(virt_addr)];
        if (pdpte & PG_PRESENT) {
            const uint64_t pde =
                    ((PageTable *)vmm_phys_to_virt(
                             vmm_table_entry_to_phys(pdpte)))
                            ->entries[vmm_virt_to_pd_index(virt_addr)];
            if (pde & PG_PRESENT) {
                const uint64_t pte =
                        ((PageTable *)vmm_phys_to_virt(
                                 vmm_table_entry_to_phys(pde)))
                                ->entries[vmm_virt_to_pt_index(virt_addr)];
                if (pte & PG_PRESENT) {
                    return pte;
                }
            }
        }
    }

    return 0;
}