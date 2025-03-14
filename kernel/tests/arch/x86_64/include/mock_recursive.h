/*
 * stage3 - Mock Recursive mapping access functions for testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_MOCK_RECURSIVE_H
#define __ANOS_KERNEL_MOCK_RECURSIVE_H

#include <stdint.h>

#ifndef RECURSIVE_ENTRY
#define RECURSIVE_ENTRY 256
#endif
#ifndef RECURSIVE_ENTRY_OTHER
#define RECURSIVE_ENTRY_OTHER (RECURSIVE_ENTRY + 1)
#endif
#ifndef KERNEL_BEGIN_ENTRY
#define KERNEL_BEGIN_ENTRY ((RECURSIVE_ENTRY + 2))
#endif

static const uintptr_t L1_LSHIFT = 39;
static const uintptr_t L2_LSHIFT = 30;
static const uintptr_t L3_LSHIFT = 21;
static const uintptr_t L4_LSHIFT = 12;
static const uintptr_t L1_RSHIFT = 12;
static const uintptr_t L2_RSHIFT = 21;
static const uintptr_t L3_RSHIFT = 30;
static const uintptr_t L4_RSHIFT = 39;
static const uintptr_t LVL_MASK = 0x1ff;

typedef struct {
    uint64_t entries[512];
} PageTable;

#ifdef MUNIT_H
extern PageTable empty_pml4;

extern PageTable complete_pml4;
extern PageTable complete_pdpt;
extern PageTable complete_pd;
extern PageTable complete_pt;

extern PageTable *current_recursive_pml4;
#else
#include <stdio.h>

#define PML4ENTRY(addr) (((unsigned short)((addr & 0x0000ff8000000000) >> 39)))
#define PDPTENTRY(addr) (((unsigned short)((addr & 0x0000007fc0000000) >> 30)))
#define PDENTRY(addr) (((unsigned short)((addr & 0x000000003fe00000) >> 21)))
#define PTENTRY(addr) (((unsigned short)((addr & 0x00000000001ff000) >> 12)))

#define MEM(arg) ((arg & ~0xfff))

PageTable empty_pml4 __attribute__((__aligned__(4096)));

PageTable complete_pml4 __attribute__((__aligned__(4096)));
PageTable complete_pdpt __attribute__((__aligned__(4096)));
PageTable complete_pd __attribute__((__aligned__(4096)));
PageTable complete_pt __attribute__((__aligned__(4096)));

PageTable *current_recursive_pml4 = &complete_pml4;

static inline uintptr_t vmm_recursive_table_address(uint16_t l1, uint16_t l2,
                                                    uint16_t l3, uint16_t l4,
                                                    uint16_t offset) {
#ifdef DEBUG_UNIT_TESTS
    printf("PLML4 @ %p\n", &complete_pml4);
    printf(" PDPT @ %p\n", &complete_pdpt);
    printf("   PD @ %p\n", &complete_pd);
    printf("   PT @ %p\n", &complete_pt);
    printf("0x%04x : 0x%04x : 0x%04x : 0x%04x :: 0x%04x", l1, l2, l3, l4,
           offset);
#endif
    PageTable *ptl4 =
            (PageTable *)(current_recursive_pml4->entries[l1] & ~(0xfff));
    PageTable *ptl3 = (PageTable *)(ptl4->entries[l2] & ~(0xfff));
    PageTable *ptl2 = (PageTable *)(ptl3->entries[l3] & ~(0xfff));
#ifdef DEBUG_UNIT_TESTS
    printf(" => %p\n", (uintptr_t)(ptl2->entries[l4]) & ~(0xfff));
#endif
    return (uintptr_t)(ptl2->entries[l4] & ~(0xfff));
}

static inline PageTable *vmm_recursive_find_pml4() { return &complete_pml4; }

static inline PageTable *vmm_recursive_find_pdpt(uint16_t pml4_entry) {
    return (PageTable *)(complete_pml4.entries[pml4_entry] & ~(0xfff));
}

static inline PageTable *vmm_recursive_find_pd(uint16_t pml4_entry,
                                               uint16_t pdpt_entry) {
    return (PageTable *)&((PageTable *)complete_pml4.entries[pml4_entry])
            ->entries[pdpt_entry];
}

static inline PageTable *vmm_recursive_find_pt(uint16_t pml4_entry,
                                               uint16_t pdpt_entry,
                                               uint16_t pd_entry) {
    return (PageTable *)&(
                   (PageTable *)MEM(
                           ((PageTable *)MEM(complete_pml4.entries[pml4_entry]))
                                   ->entries[pdpt_entry]))
            ->entries[pd_entry];
}

static inline uint64_t *vmm_virt_to_pte(uintptr_t virt_addr) {
    return (uint64_t *)&(
                   (PageTable *)MEM(
                           ((PageTable *)MEM(
                                    ((PageTable *)MEM(
                                             complete_pml4.entries[PML4ENTRY(
                                                     virt_addr)]))
                                            ->entries[PDPTENTRY(virt_addr)]))
                                   ->entries[PDENTRY(virt_addr)]))
            ->entries[PTENTRY(virt_addr)];
}

static inline PageTable *vmm_virt_to_pt(uintptr_t virt_addr) {
    return (PageTable *)&(
                   (PageTable *)MEM(
                           ((PageTable *)MEM(complete_pml4.entries[PML4ENTRY(
                                    virt_addr)]))
                                   ->entries[PDPTENTRY(virt_addr)]))
            ->entries[PDENTRY(virt_addr)];
}

static inline uint64_t *vmm_virt_to_pde(uintptr_t virt_addr) {
    return (uint64_t *)&(
                   (PageTable *)MEM(
                           ((PageTable *)MEM(complete_pml4.entries[PML4ENTRY(
                                    virt_addr)]))
                                   ->entries[PDPTENTRY(virt_addr)]))
            ->entries[PDENTRY(virt_addr)];
}

static inline PageTable *vmm_virt_to_pd(uintptr_t virt_addr) {
    return (PageTable *)&((PageTable *)MEM(
                                  complete_pml4.entries[PML4ENTRY(virt_addr)]))
            ->entries[PDPTENTRY(virt_addr)];
}

static inline uint64_t *vmm_virt_to_pdpte(uintptr_t virt_addr) {
    return (uint64_t *)&((PageTable *)MEM(
                                 complete_pml4.entries[PML4ENTRY(virt_addr)]))
            ->entries[PDPTENTRY(virt_addr)];
}

static inline PageTable *vmm_virt_to_pdpt(uintptr_t virt_addr) {
    return (PageTable *)&complete_pml4.entries[PML4ENTRY(virt_addr)];
}

static inline uint64_t *vmm_virt_to_pml4e(uintptr_t virt_addr) {
    return &complete_pml4.entries[PML4ENTRY(virt_addr)];
}

static inline PageTable *vmm_virt_to_pml4(uintptr_t virt_addr) {
    return &complete_pml4;
}

static inline uint16_t
vmm_recursive_pml4_virt_to_recursive_entry(void *virt_pml4) {
    return 256;
}

static inline uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr) { return 0; }

static inline uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr) { return 0; }

static inline uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr) { return 0; }

static inline uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr) { return 0; }

static inline uintptr_t vmm_virt_to_phys_page(uintptr_t virt_addr) { return 0; }

static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) { return 0; }
#endif
#endif //__ANOS_KERNEL_MOCK_RECURSIVE_H
