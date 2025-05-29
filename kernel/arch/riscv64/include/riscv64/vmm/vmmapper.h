/*
 * RISC-V Memory Management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H
#define __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "machine.h"
#include "riscv64/kdrivers/cpu.h"
#include "vmm/vmmapper.h"

/*
 * First PML4 entry of kernel space
 */
#define FIRST_KERNEL_PML4E ((256))

// Page table entry flags
#define PG_PRESENT (1ULL << 0)  // Valid
#define PG_READ (1ULL << 1)     // Read
#define PG_WRITE (1ULL << 2)    // Write
#define PG_EXEC (1ULL << 3)     // Execute
#define PG_USER (1ULL << 4)     // User
#define PG_GLOBAL (1ULL << 5)   // Global
#define PG_ACCESSED (1ULL << 6) // Accessed
#define PG_DIRTY (1ULL << 7)    // Dirty

/*
 * Page COW attribute (STAGE3-specific)
 */
#define PG_COPY_ON_WRITE ((1 << 8))

/*
 * riscv64 does not have a "PAGE_SIZE" bit, it's implied by an entry
 * being a leaf node (having any of read/write/user set).
 *
 * Just define zero so it has no effect.
 */
#define PG_PAGESIZE ((0))

#define PAGE_TABLE_ENTRIES ((512))

// Just used to page-align addresses to their containing page
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))

// Just used to extract page-relative addresses from their containing page
#define PAGE_RELATIVE_MASK (~PAGE_ALIGN_MASK)

// Just used to extract PTE flags
#define PAGE_FLAGS_MASK PAGE_RELATIVE_MASK

// Base of the per-CPU temporary mapping pages
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))

#define IS_USER_ADDRESS(ptr) (((((uint64_t)(ptr)) & 0xffff800000000000) == 0))

typedef struct {
    uint64_t entries[PAGE_TABLE_ENTRIES];
} PageTable;

// Convert physical address to direct-mapped virtual address
static inline uintptr_t vmm_phys_to_virt(uintptr_t phys_addr) {
    return DIRECT_MAP_BASE + phys_addr;
}

static inline void *vmm_phys_to_virt_ptr(uintptr_t phys_addr) {
    return (void *)vmm_phys_to_virt(phys_addr);
}

// Convert direct-mapped virtual address to physical address
static inline uintptr_t vmm_virt_to_phys(uintptr_t virt_addr) {
    return virt_addr - DIRECT_MAP_BASE;
}

static inline uintptr_t vmm_virt_to_phys_page(const uintptr_t virt_addr) {
    return vmm_virt_to_phys(virt_addr) & PAGE_ALIGN_MASK;
}

static inline PageTable *vmm_find_pml4() {
    return (PageTable *)vmm_phys_to_virt_ptr(
            cpu_satp_to_root_table_phys(cpu_read_satp()));
}

static inline uint16_t vmm_virt_to_table_index(uintptr_t virt_addr,
                                               uint8_t level) {
    return ((virt_addr >> ((9 * (level - 1)) + 12)) & 0x1ff);
}

static inline uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 9 + 9 + 12)) & 0x1ff);
}

static inline uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 9 + 12)) & 0x1ff);
}

static inline uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr) {
    return ((virt_addr >> (9 + 12)) & 0x1ff);
}

static inline uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr) {
    return ((virt_addr >> 12) & 0x1ff);
}

static inline uintptr_t vmm_table_entry_to_phys(uintptr_t table_entry) {
    return ((table_entry >> 10) << 12);
}

static inline uint16_t vmm_table_entry_to_page_flags(uintptr_t table_entry) {
    return (uint16_t)(table_entry & 0x3ff);
}

static inline uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys,
                                                         uint64_t flags) {
    return ((phys & ~0xFFF) >> 2) | flags;
}

static inline size_t vmm_level_page_size(uint8_t level) {
    return (VM_PAGE_SIZE << (9 * (level - 1)));
}

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
static inline uintptr_t vmm_per_cpu_temp_page_addr(uint8_t cpu) {
    return PER_CPU_TEMP_PAGE_BASE + (cpu << 12);
}

// Initialize the direct mapping for physical memory
// This must be called during early boot, before SMP
// or userspace is up (since it abuses both those things)
void vmm_init_direct_mapping(uint64_t *pml4, Limine_MemMap *memmap);

#endif // __ANOS_KERNEL_ARCH_RISCV64_VMM_VMMAPPER_H