/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "pmm/pagealloc.h"
#include "std/string.h"
#include "vmm/vmmapper.h"
#include "x86_64/kdrivers/cpu.h"

#ifdef UNIT_TESTS
#include "mock_cpu.h"
#else
#include "x86_64/kdrivers/cpu.h"
#endif

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

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr yet - May 2025
#ifndef nullptr
#ifdef nullptr
#define nullptr nullptr
#else
#define nullptr (((void *)0))
#endif
#endif
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
 * Returns true if the given entry is a large-sized leaf, i.e.
 * has the PAGE_SIZE flag set.
 */
STATIC_EXCEPT_TESTS bool is_pagesize_leaf(const uint64_t table_entry) {
    return table_entry & (PG_PAGESIZE);
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
                                            const PagetableLevel to_level,
                                            const uint16_t flags) {

    if (to_level < 1 || to_level > 4) {
        return nullptr;
    }

    uint8_t levels_remain = 4 - to_level;
    uint64_t *current_table = (uint64_t *)root_table;

    while (levels_remain) {
        const uint16_t current_index =
                vmm_virt_to_table_index(virt_addr, levels_remain + 1);
        const uint64_t current_entry = current_table[current_index];

        if ((current_entry & PG_PRESENT) == 0) {
            const uintptr_t new_table = page_alloc(physical_region);

            if (new_table & 0xfff) {
                // TODO unmap what we mapped so far? Is it worth it?
                return nullptr;
            }

            uint64_t *new_ptr = vmm_phys_to_virt_ptr(new_table);

            memclr(new_ptr, VM_PAGE_SIZE);

            current_table[current_index] = vmm_phys_and_flags_to_table_entry(
                    new_table, PG_PRESENT | (flags & PG_WRITE ? PG_WRITE : 0) |
                                       (flags & PG_USER ? PG_USER : 0));

            current_table = new_ptr;
        } else {
            // special case - if this is a leaf, but we wanted a lower level,
            // we'll just fail...
            if (levels_remain > 1 && is_pagesize_leaf(current_entry)) {
                return nullptr;
            }

            // x86_64 requires writeable leaf pages have write set on
            // all parent pages, because why wouldn't it 🙄
            current_table[current_index] = vmm_phys_and_flags_to_table_entry(
                    vmm_table_entry_to_phys(current_entry),
                    vmm_table_entry_to_page_flags(current_entry) |
                            (flags & PG_WRITE ? PG_WRITE : 0) |
                            (flags & PG_USER ? PG_USER : 0));

            current_table = vmm_phys_to_virt_ptr(
                    vmm_table_entry_to_phys(current_entry));
        }

        levels_remain -= 1;
    }

    return current_table;
}

inline void vmm_invalidate_page(const uintptr_t virt_addr) {
    cpu_invalidate_tlb_addr(virt_addr);
}

static bool nolock_vmm_map_page_containing_in(uint64_t *pml4,
                                              const uintptr_t virt_addr,
                                              const uint64_t phys_addr,
                                              const uint16_t flags) {
    const uintptr_t aligned_virt_addr = virt_addr & PAGE_ALIGN_MASK;

    uint64_t *pt = ensure_tables(pml4, aligned_virt_addr, PT_LEVEL_PT, flags);

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
    return vmm_map_page_containing_in(vmm_phys_to_virt_ptr(cpu_read_cr3()),
                                      virt_addr, phys_addr, flags);
}

inline bool vmm_map_page_in(uint64_t *pml4, const uintptr_t virt_addr,
                            const uint64_t page, const uint16_t flags) {
    return vmm_map_page_containing_in(pml4, virt_addr, page, flags);
}

inline bool vmm_map_page(const uintptr_t virt_addr, const uint64_t page,
                         const uint16_t flags) {
    return vmm_map_page_containing(virt_addr, page, flags);
}

inline bool vmm_map_pages_containing_in(uint64_t *pml4, uintptr_t virt_addr,
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

    return vmm_map_pages_containing_in(vmm_phys_to_virt_ptr(cpu_read_cr3()),
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

static uintptr_t nolock_vmm_unmap_page_in(uint64_t *pml4,
                                          const uintptr_t virt_addr) {
    const uint16_t pml4_index = vmm_virt_to_pml4_index(virt_addr);
    const uint64_t pml4e = pml4[pml4_index];

    if (pml4e & PG_PRESENT) {
        if (is_pagesize_leaf(pml4e)) {
            // unmapping a terapage
            pml4[pml4_index] = 0;

            vmm_invalidate_page(virt_addr);

            return vmm_table_entry_to_phys(pml4e);
        }

        uint64_t *pdpt = vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pml4e));
        const uint16_t pdpt_index = vmm_virt_to_pdpt_index(virt_addr);
        const uint64_t pdpte = pdpt[pdpt_index];

        if (pdpte & PG_PRESENT) {
            if (is_pagesize_leaf(pdpte)) {
                // unmapping a gigapage
                pdpt[pdpt_index] = 0;

                vmm_invalidate_page(virt_addr);

                return vmm_table_entry_to_phys(pdpte);
            }

            uint64_t *pd = vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pdpte));
            const uint16_t pd_index = vmm_virt_to_pd_index(virt_addr);
            const uint64_t pde = pd[pd_index];

            if (pde & PG_PRESENT) {
                if (is_pagesize_leaf(pde)) {
                    // unmapping a megapage
                    pd[pd_index] = 0;
                    vmm_invalidate_page(virt_addr);

                    return vmm_table_entry_to_phys(pde);
                }

                uint64_t *pt =
                        vmm_phys_to_virt_ptr(vmm_table_entry_to_phys(pde));
                const uint16_t pt_index = vmm_virt_to_pt_index(virt_addr);
                const uint64_t pte = pt[pt_index];

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

inline uintptr_t vmm_unmap_page_in(uint64_t *pml4, const uintptr_t virt_addr) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    const uintptr_t result = nolock_vmm_unmap_page_in(pml4, virt_addr);
    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return result;
}

inline uintptr_t vmm_unmap_page(const uintptr_t virt_addr) {
    return vmm_unmap_page_in(vmm_phys_to_virt_ptr(cpu_read_cr3()), virt_addr);
}

inline uintptr_t vmm_unmap_pages_in(uint64_t *pml4, const uintptr_t virt_addr,
                                    const size_t num_pages) {
    const uint64_t lock_flags = spinlock_lock_irqsave(&vmm_map_lock);
    const uintptr_t result = nolock_vmm_unmap_page_in(pml4, virt_addr);

    for (int i = 1; i < num_pages; i++) {
        nolock_vmm_unmap_page_in(pml4, virt_addr + (i << VM_PAGE_LINEAR_SHIFT));
    }

    spinlock_unlock_irqrestore(&vmm_map_lock, lock_flags);
    return result;
}

uintptr_t vmm_unmap_pages(const uintptr_t virt_addr, const size_t num_pages) {
    return vmm_unmap_pages_in(vmm_phys_to_virt_ptr(cpu_read_cr3()), virt_addr,
                              num_pages);
}

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
inline uintptr_t vmm_per_cpu_temp_page_addr(const uint8_t cpu) {
    return PER_CPU_TEMP_PAGE_BASE + (cpu << 12);
}

// Convert physical address to direct-mapped virtual address
inline uintptr_t vmm_phys_to_virt(const uintptr_t phys_addr) {
    return DIRECT_MAP_BASE + phys_addr;
}

inline void *vmm_phys_to_virt_ptr(const uintptr_t phys_addr) {
    return (void *)vmm_phys_to_virt(phys_addr);
}

inline uintptr_t vmm_virt_to_phys_page(const uintptr_t virt_addr) {
    return vmm_virt_to_phys(virt_addr) & PAGE_ALIGN_MASK;
}

inline PageTable *vmm_find_pml4() {
    return (PageTable *)vmm_phys_to_virt_ptr(cpu_read_cr3());
}

inline uint16_t vmm_virt_to_table_index(const uintptr_t virt_addr,
                                        const uint8_t level) {
    return ((virt_addr >> ((9 * (level - 1)) + 12)) & 0x1ff);
}

inline uint16_t vmm_virt_to_pml4_index(const uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 9 + 9 + 12)) & 0x1ff);
}

inline uint16_t vmm_virt_to_pdpt_index(const uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 9 + 12)) & 0x1ff);
}

inline uint16_t vmm_virt_to_pd_index(const uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 12)) & 0x1ff);
}

inline uint16_t vmm_virt_to_pt_index(const uintptr_t virt_addr) {
    return ((virt_addr >> 12) & 0x1ff);
}

inline uintptr_t vmm_table_entry_to_phys(const uintptr_t table_entry) {
    return (table_entry & 0x0000fffffffff000);
}

inline uint16_t vmm_table_entry_to_page_flags(const uintptr_t table_entry) {
    return (uint16_t)(table_entry & 0x3ff);
}

inline uint64_t vmm_phys_and_flags_to_table_entry(const uintptr_t phys,
                                                  const uint64_t flags) {
    return ((phys & ~0xfff)) | flags;
}

/*
 * Get the PT entry (including flags) for the given virtual address,
 * or 0 if not mapped in the _current process_ direct mapping.
 *
 * This **only** works for 4KiB pages - large pages will not work
 * with this (and that's by design!)
 */
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

// Convert virtual address to phys via table walk
uintptr_t vmm_virt_to_phys(const uintptr_t virt_addr) {
    return vmm_table_entry_to_phys(vmm_virt_to_pt_entry(virt_addr));
}

size_t vmm_level_page_size(const uint8_t level) {
    return (VM_PAGE_SIZE << (9 * (level - 1)));
}