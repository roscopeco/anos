/*
 * stage3 - Recursive mapping access functions
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_VM_RECURSIVE_H
#define __ANOS_KERNEL_VM_RECURSIVE_H

#include <stdint.h>

// Base address for tables (high bits always set, tables will always be in kernel space...)
#define BASE_ADDRESS (((uintptr_t)0xffff000000000000))

// Mask to extract just the table selector bits from a virtual address
#define TABLE_BIT_MASK ((0x0000fffffffff000))

// Mask to remove the offset (bottom 12 bits) from an address
#define OFFSET_MASK ((0xfffffffffffff000))

#define L1_LSHIFT ((39)) // Amount to shift a L1 index left when building
#define L2_LSHIFT ((30)) // Amount to shift a L2 index left when building
#define L3_LSHIFT ((21)) // Amount to shift a L3 index left when building
#define L4_LSHIFT ((12)) // Amount to shift a L4 index left when building

#define L1_RSHIFT ((9))  // Amount to shift a vaddr right when extracting L1
#define L2_RSHIFT ((18)) // Amount to shift a vaddr right when extracting L2
#define L3_RSHIFT ((27)) // Amount to shift a vaddr right when extracting L3
#define L4_RSHIFT ((36)) // Amount to shift a vaddr right when extracting L4

// Index of recursive mapping entry in PML4
// `0xffff800000000000`-> `0xffff807fffffffff` : 512GB Recursive mapping area when @ PML4[256]
#define RECURSIVE_ENTRY ((256))

// Internal stuff...
#define LVL_MASK ((0x1ff)) // Mask to apply to a table index
#define OFS_MASK ((0xfff)) // Mask to apply to a page offset

// Fixed parts of addresses used when building table access addresses for virtual addresses
// at various levels. These are all precomputed and should just end up as constants...
//
#define RECURSIVE_L1 ((((uintptr_t)RECURSIVE_ENTRY) << L1_LSHIFT))
#define RECURSIVE_L2                                                           \
    ((RECURSIVE_L1 | (((uintptr_t)RECURSIVE_ENTRY) << L2_LSHIFT)))
#define RECURSIVE_L3                                                           \
    ((RECURSIVE_L1 | RECURSIVE_L2 |                                            \
      (((uintptr_t)RECURSIVE_ENTRY) << L3_LSHIFT)))
#define RECURSIVE_L4                                                           \
    ((RECURSIVE_L1 | RECURSIVE_L2 | RECURSIVE_L3 |                             \
      (((uintptr_t)RECURSIVE_ENTRY) << L4_LSHIFT)))

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
 * uintptr_t pml4 = vmm_recursive_table_address(511, 511, 511, 511, 0);
 * ```
 *
 * To get a PDPT (from PML4 index 1 in this case):
 *
 * ```C
 * uintptr_t pdpt1 = vmm_recursive_table_address(511, 511, 511, 1, 0);
 * ```
 *
 * Or a PD, index 2, from PDPT index 1:
 *
 * ```C
 * uintptr_t pdpt1_pd2 = vmm_recursive_table_address(511, 511, 1, 2, 0);
 * ```
 *
 * And finally a PT, index 3 from PD index 2, PDPT index 1:
 *
 * ```C
 * uintptr_t pdpt1_pd2_pt3 = vmm_recursive_table_address(511, 1, 2, 3, 0);
 * ```
 *
 * If an alternative PML4 entry is being used for the recursive mapping,
 * that can be specified here too (unlike most of the rest of the functions
 * in this file, which are hardcoded to use the last entry):
 *
 * ```C
 * uintptr_t pdpt1_pd2_pt3 = vmm_recursive_table_address(500, 1, 2, 3, 0);
 * ```
 *
 * Note that this function **does not** canonicalise addresses automatically - so the
 * l1 value must translate to addresses above 0xffff800000000000.
 *
 * In practice, this means that the recursive mapping must be in the top 256 entries
 * of the PML4 - the kernel maps the _current_ PML4 into the second-from-top (index 510)
 * entry, and I plan to map alternative process spaces into the third-from-top (index 509)
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
    return (PageTable *)(BASE_ADDRESS | RECURSIVE_L4);
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
                        ((virt_addr & TABLE_BIT_MASK) >> L1_RSHIFT));
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

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L2 |
                        ((virt_addr & TABLE_BIT_MASK) >> L2_RSHIFT));
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

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L3 |
                        ((virt_addr & TABLE_BIT_MASK) >> L3_RSHIFT));
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

    return (uint64_t *)(BASE_ADDRESS | RECURSIVE_L4 |
                        ((virt_addr & TABLE_BIT_MASK) >> L4_RSHIFT));
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

#endif //__ANOS_KERNEL_VM_RECURSIVE_H
