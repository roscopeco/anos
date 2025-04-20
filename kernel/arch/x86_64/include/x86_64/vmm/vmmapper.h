/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 * 
 * This is the platform-agnostic interface. It will include
 * the platform-specific parts from arch/$ARCH/include.
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H
#define __ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vmm/vmconfig.h"

/*
 * Extract a PML4 index from a virtual address.
 */
#define PML4ENTRY(addr) (((unsigned short)((addr & 0x0000ff8000000000) >> 39)))

/*
 * Extract a PDPT index from a virtual address.
 */
#define PDPTENTRY(addr) (((unsigned short)((addr & 0x0000007fc0000000) >> 30)))

/*
 * Extract a PD index from a virtual address.
 */
#define PDENTRY(addr) (((unsigned short)((addr & 0x000000003fe00000) >> 21)))

/*
 * Extract a PT index from a virtual address.
 */
#define PTENTRY(addr) (((unsigned short)((addr & 0x00000000001ff000) >> 12)))

/*
 * Page present attribute
 */
#define PG_PRESENT ((1 << 0))

/*
 * Page writeable attribute
 */
#define PG_WRITE ((1 << 1))

/*
 * Page user attribute
 */
#define PG_USER ((1 << 2))

/*
 * Page COW attribute (STAGE3-specific)
 */
#define PG_COPY_ON_WRITE ((1 << 6))

/*
 * Page size attribute (for large pages)
 */
#define PG_PAGESIZE ((1 << 7))

// This is where we map the PMM region(s)
#define STATIC_KERNEL_SPACE ((0xFFFFFFFF80000000))

// Just used to page-align addresses to their containing page
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))

// Just used to extract page-relative addresses from their containing page
#define PAGE_RELATIVE_MASK (~PAGE_ALIGN_MASK)

// Just used to extract PTE flags
#define PAGE_FLAGS_MASK PAGE_RELATIVE_MASK

// Base of the per-CPU temporary mapping pages
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))

#define IS_USER_ADDRESS(ptr) ((((uint64_t)ptr & 0xffff800000000000) == 0))

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
static inline uintptr_t vmm_per_cpu_temp_page_addr(uint8_t cpu) {
    return PER_CPU_TEMP_PAGE_BASE + (cpu << 12);
}

#ifdef UNIT_TESTS
#include "mock_recursive.h"
#else
#include "x86_64/vmm/recursive_paging.h"
#endif

static inline uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr) {
    return (virt_addr & (LVL_MASK << L1_LSHIFT)) >> L4_RSHIFT;
}

static inline uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr) {
    return (virt_addr & (LVL_MASK << L2_LSHIFT)) >> L3_RSHIFT;
}

static inline uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr) {
    return (virt_addr & (LVL_MASK << L3_LSHIFT)) >> L2_RSHIFT;
}

static inline uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr) {
    return (virt_addr & (LVL_MASK << L4_LSHIFT)) >> L1_RSHIFT;
}

static inline uint16_t vmm_virt_to_table_index(uintptr_t virt_addr,
                                               uint8_t level) {
    return ((virt_addr >> ((9 * (level - 1)) + 12)) & 0x1ff);
}

static inline size_t vmm_level_page_size(uint8_t level) {
    return (VM_PAGE_SIZE << (9 * (level - 1)));
}

static inline uintptr_t vmm_table_entry_to_phys(uintptr_t table_entry) {
    return table_entry & PAGE_ALIGN_MASK;
}

static inline uint16_t vmm_table_entry_to_page_flags(uintptr_t table_entry) {
    return table_entry & PAGE_FLAGS_MASK;
}

static inline uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys,
                                                         uint64_t flags) {
    return (phys & ~0xFFF) | flags;
}
#endif //__ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H
