/*
 * stage3 - Mock Recursive mapping access functions for testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_MOCK_RECURSIVE_H
#define __ANOS_KERNEL_MOCK_RECURSIVE_H

#include <stdint.h>

#define RECURSIVE_ENTRY 256

typedef struct {
    uint64_t entries[512];
} PageTable;

#ifdef MUNIT_H
extern PageTable empty_pml4;

extern PageTable complete_pml4;
extern PageTable complete_pdpt;
extern PageTable complete_pd;
extern PageTable complete_pt;
#else

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

#endif
#endif //__ANOS_KERNEL_MOCK_RECURSIVE_H
