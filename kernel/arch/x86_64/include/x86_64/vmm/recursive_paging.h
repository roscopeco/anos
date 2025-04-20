/*
 * stage3 - Recursive mapping access functions
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_VM_RECURSIVE_H
#define __ANOS_KERNEL_ARCH_X86_64_VM_RECURSIVE_H

#include <stdint.h>

// TODO it'd be nice if these were constexpr but then we can't build hosted
//      tests (Apple clang doesn't support constexpr yet - Jan 2025)
//

// Base address for tables (high bits always set, tables will always be in kernel space...)
static const uintptr_t BASE_ADDRESS = 0xffff000000000000;

// Mask to extract just the table selector bits from a virtual address
static const uintptr_t TABLE_BIT_MASK = 0x0000fffffffff000;

// Mask to remove the offset (bottom 12 bits) from an address
static const uintptr_t OFFSET_MASK = 0xfffffffffffff000;

// Amount to shift a L1 index left when building
static const uintptr_t L1_LSHIFT = 39;

// Amount to shift a L2 index left when building
static const uintptr_t L2_LSHIFT = 30;

// Amount to shift a L3 index left when building
static const uintptr_t L3_LSHIFT = 21;

// Amount to shift a L4 index left when building
static const uintptr_t L4_LSHIFT = 12;

// Amount to shift a vaddr right when extracting L1
// 3 more than you'd expect, we shift back by 3 to
// fix up the offset to a direct address.
static const uintptr_t L1_RSHIFT = 12;

// Amount to shift a vaddr right when extracting L2
// 3 more than you'd expect, we shift back by 3 to
// fix up the offset to a direct address.
static const uintptr_t L2_RSHIFT = 21;

// Amount to shift a vaddr right when extracting L3
// 3 more than you'd expect, we shift back by 3 to
// fix up the offset to a direct address.
static const uintptr_t L3_RSHIFT = 30;

// Amount to shift a vaddr right when extracting L4
// 3 more than you'd expect, we shift back by 3 to
// fix up the offset to a direct address.
static const uintptr_t L4_RSHIFT = 39;

// Index of recursive mapping entry in PML4
// `0xffff800000000000`-> `0xffff807fffffffff` : 512GB Recursive mapping area when @ PML4[256]
static const uintptr_t RECURSIVE_ENTRY_MAIN = 256;

// `0xffff808000000000`-> `0xffff80ffffffffff` : 512GB Recursive mapping area when @ PML4[257]
static const uintptr_t RECURSIVE_ENTRY_OTHER = 257;

// MAIN is the primary recursive entry that will be used by default...
static const uintptr_t RECURSIVE_ENTRY = RECURSIVE_ENTRY_MAIN;

static const uintptr_t KERNEL_BEGIN_ENTRY = RECURSIVE_ENTRY + 2;

// Internal stuff...
static const uintptr_t LVL_MASK = 0x1ff; // Mask to apply to a table index
static const uintptr_t OFS_MASK = 0xfff; // Mask to apply to a page offset

// Fixed parts of addresses used when building table access addresses for virtual addresses
// at various levels. These are all precomputed and should just end up as constants...
//
static const uintptr_t RECURSIVE_L1 = RECURSIVE_ENTRY << L1_LSHIFT;
static const uintptr_t RECURSIVE_L2 = RECURSIVE_ENTRY << L2_LSHIFT;
static const uintptr_t RECURSIVE_L3 = RECURSIVE_ENTRY << L3_LSHIFT;
static const uintptr_t RECURSIVE_L4 = RECURSIVE_ENTRY << L4_LSHIFT;

typedef struct {
    uint64_t entries[512];
} PageTable;

/*
 * Build a virtual address to access a specific page table and (byte) offset within
 * recursive mappings. This is kinda low-level, the other `vmm_recursive` functions build
 * on this and should usually be used instead.
 *
 * The tests have some example usage, but generally you use this by specifying the entry
 * to use for each level of the page tables, repeating the recursive mapping index as
 * often as needed for the appropriate table level you're after.
 *
 * For example, to get the PML4 itself, just specify the recursive mapping for all
 * levels, and a zero offset:
 *
 * ```C
 * uintptr_t pml4 = vmm_recursive_table_address(256, 256, 256, 256, 0);
 * ```
 *
 * To get a PDPT (from PML4 index 1 in this case):
 *
 * ```C
 * uintptr_t pdpt1 = vmm_recursive_table_address(256, 256, 256, 1, 0);
 * ```
 *
 * Or a PD, index 2, from PDPT index 1:
 *
 * ```C
 * uintptr_t pdpt1_pd2 = vmm_recursive_table_address(256, 256, 1, 2, 0);
 * ```
 *
 * And finally a PT, index 3 from PD index 2, PDPT index 1:
 *
 * ```C
 * uintptr_t pdpt1_pd2_pt3 = vmm_recursive_table_address(256, 1, 2, 3, 0);
 * ```
 *
 * If an alternative PML4 entry is being used for the recursive mapping,
 * that can be specified here too (unlike most of the rest of the functions
 * in this file, which are hardcoded to use RECURSIVE_ENTRY):
 *
 * ```C
 * uintptr_t pdpt1_pd2_pt3 = vmm_recursive_table_address(257, 1, 2, 3, 0);
 * ```
 *
 * Note that this function **does not** canonicalise addresses automatically - so the
 * l1 value must translate to addresses above 0xffff800000000000.
 *
 * In practice, this means that the recursive mapping must be in the top 256 entries
 * of the PML4 - the kernel maps the _current_ PML4 into the first such entry (index 256)
 * entry, and I plan to map alternative process spaces into the next entry (index 257)
 * if/when that becomes a thing, so this isn't a problem for now...
 */
static inline uintptr_t vmm_recursive_table_address(uint16_t l1, uint16_t l2,
                                                    uint16_t l3, uint16_t l4,
                                                    uint16_t offset) {
    return BASE_ADDRESS | (((uintptr_t)l1 & LVL_MASK) << L1_LSHIFT) |
           (((uintptr_t)l2 & LVL_MASK) << L2_LSHIFT) |
           (((uintptr_t)l3 & LVL_MASK) << L3_LSHIFT) |
           (((uintptr_t)l4 & LVL_MASK) << L4_LSHIFT) |
           ((uintptr_t)offset & OFS_MASK);
}

/*
 * Find the PML4 using the _current process'_ recursive mapping (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_recursive_find_pml4() {
    return (PageTable *)(BASE_ADDRESS | RECURSIVE_L1 | RECURSIVE_L2 |
                         RECURSIVE_L3 | RECURSIVE_L4);
}

/*
 * Find a given PDPT using the _current process'_ recursive mapping (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_recursive_find_pdpt(uint16_t pml4_entry) {
    return (PageTable *)vmm_recursive_table_address(
            RECURSIVE_ENTRY, RECURSIVE_ENTRY, RECURSIVE_ENTRY, pml4_entry, 0);
}

/*
 * Find a given PD using the _current process_ recursive mapping (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_recursive_find_pd(uint16_t pml4_entry,
                                               uint16_t pdpt_entry) {
    return (PageTable *)vmm_recursive_table_address(
            RECURSIVE_ENTRY, RECURSIVE_ENTRY, pml4_entry, pdpt_entry, 0);
}

/*
 * Find the PT using the _current process_ recursive mapping (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_recursive_find_pt(uint16_t pml4_entry,
                                               uint16_t pdpt_entry,
                                               uint16_t pd_entry) {
    return (PageTable *)vmm_recursive_table_address(RECURSIVE_ENTRY, pml4_entry,
                                                    pdpt_entry, pd_entry, 0);
}

/*
 * Find the PTE mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline uint64_t *vmm_virt_to_pte(uintptr_t virt_addr) {

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0x0000008080604000 :: 0b0000000000000000 000000001 000000010 000000011 000000100 000000000000
    // ... becomes ...
    // 0xffff800040403020 :: 0b1111111111111111 100000000 000000001 000000010 000000011 000000100000

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L1 |
                        ((virt_addr & TABLE_BIT_MASK) >> L1_RSHIFT << 3));
}

/*
 * Find the PT mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_virt_to_pt(uintptr_t virt_addr) {
    return (PageTable *)((uintptr_t)vmm_virt_to_pte(virt_addr) & OFFSET_MASK);
}

/*
 * Find the PDE mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline uint64_t *vmm_virt_to_pde(uintptr_t virt_addr) {

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0x0000008080604000 :: 0b0000000000000000 000000001 000000010 000000011 000000100 000000000000
    // ... becomes ...
    // 0xffffffffc0202018 :: 0b1111111111111111 111111111 111111111 000000001 000000010 000000011000
    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L1 | RECURSIVE_L2 |
                        ((virt_addr & TABLE_BIT_MASK) >> L2_RSHIFT << 3));
}

/*
 * Find the PD mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_virt_to_pd(uintptr_t virt_addr) {
    return (PageTable *)((uintptr_t)vmm_virt_to_pde(virt_addr) & OFFSET_MASK);
}

/*
 * Find the PDPTE mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline uint64_t *vmm_virt_to_pdpte(uintptr_t virt_addr) {

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0x0000008080604000 :: 0b0000000000000000 000000001 000000010 000000011 000000100 000000000000
    // ... becomes ...
    // 0xffffffffffe01010 :: 0b1111111111111111 111111111 111111111 111111111 000000001 000000010000

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L1 | RECURSIVE_L2 |
                        RECURSIVE_L3 |
                        ((virt_addr & TABLE_BIT_MASK) >> L3_RSHIFT << 3));
}

/*
 * Find the PDPT mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline PageTable *vmm_virt_to_pdpt(uintptr_t virt_addr) {
    return (PageTable *)((uintptr_t)vmm_virt_to_pdpte(virt_addr) & OFFSET_MASK);
}

/*
 * Find the PML4E mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 */
static inline uint64_t *vmm_virt_to_pml4e(uintptr_t virt_addr) {

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0x0000008080604000 :: 0b0000000000000000 000000001 000000010 000000011 000000100 000000000000
    // ... becomes ...
    // 0xfffffffffffff008 :: 0b1111111111111111 111111111 111111111 111111111 111111111 000000001000

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L1 | RECURSIVE_L2 |
                        RECURSIVE_L3 | RECURSIVE_L4 |
                        ((virt_addr & TABLE_BIT_MASK) >> L4_RSHIFT << 3));
}

/*
 * Find the PML4 mapping the given virtual address using the _current process_ recursive mapping
 * (specified by `RECURSIVE_ENTRY`).
 *
 * This is provided for completeness really, it will return the same fixed address everywhere.
 */
static inline PageTable *vmm_virt_to_pml4(uintptr_t virt_addr) {
    return (PageTable *)((uintptr_t)vmm_virt_to_pml4e(virt_addr) & OFFSET_MASK);
}

/*
 * Get the PT entry (including flags) for the given virtual address,
 * or 0 if not mapped in the _current process_ recursive mapping.
 * 
 * This **only** works for 4KiB pages - large pages will not work 
 * with this (and that's by design!)
 */
static inline uint64_t vmm_virt_to_pt_entry(uintptr_t virt_addr) {
    uint64_t pml4e = *vmm_virt_to_pml4e(virt_addr);
    if (pml4e & 0x1) {
        uint64_t pdpte = *vmm_virt_to_pdpte(virt_addr);
        if (pdpte & 0x1) {
            uint64_t pde = *vmm_virt_to_pde(virt_addr);
            if (pde & 0x1) {
                uint64_t pte = *vmm_virt_to_pte(virt_addr);
                if (pte & 0x1) {
                    return pte;
                }
            }
        }
    }

    return 0;
}

/*
 * Get the physical base address of the page containing the given virtual address, or 
 * 0 if the page is not mapped in the _current process_ recursive mapping.
 * 
 * TODO if we ever start making low RAM available for mapping, this will have to change...
 * 
 * TODO we're gonna need this for the 'other' address space too...
 */
static inline uintptr_t vmm_virt_to_phys_page(uintptr_t virt_addr) {
    uint64_t pte = vmm_virt_to_pt_entry(virt_addr);

    if (pte) {
        return pte & OFFSET_MASK;
    }

    return 0;
}

/*
 * Get the physical address corresponding to the given virtual address, or 
 * 0 if the page is not mapped in the _current process_ recursive mapping.
 * 
 * TODO if we ever start making low RAM available for mapping, this will have to change...
 */
static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) {
    uintptr_t page = vmm_virt_to_phys_page(virt_addr);

    if (page) {
        return page | (virt_addr & ~OFFSET_MASK);
    } else {
        return 0;
    }
}

static inline uint16_t
vmm_recursive_pml4_virt_to_recursive_entry(void *virt_pml4) {
    return (((uintptr_t)virt_pml4) & (LVL_MASK << L4_LSHIFT)) >> L1_RSHIFT;
}

#endif //__ANOS_KERNEL_ARCH_X86_64_VM_RECURSIVE_H
