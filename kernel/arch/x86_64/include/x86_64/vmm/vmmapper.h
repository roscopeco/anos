/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 * 
 * This is the platform-agnostic interface. It will include
 * the platform-specific parts from arch/$ARCH/include.
 */

// clang-format Language: Cpp

#ifndef __ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H
#define __ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "machine.h"
#include "vmm/vmconfig.h"
#include "x86_64/kdrivers/cpu.h"

/*
 * First PML4 entry of kernel space
 */
#define FIRST_KERNEL_PML4E ((256))

/*
 * Page present attribute
 */
#define PG_PRESENT ((1ULL << 0))

/*
 * Page writeable attribute
 */
#define PG_WRITE ((1ULL << 1))

/*
 * Page user attribute
 */
#define PG_USER ((1ULL << 2))

/*
 * Page write-through attribute
 */
#define PG_WRITETHROUGH ((1ULL << 3))

/*
 * Page cache disable attribute
 */
#define PG_NOCACHE ((1ULL << 4))

/*
 * Page XD (Execute-disable)
 */
#define PG_NOEXEC ((1ULL << 63))

/*
 * Page size attribute (for large pages)
 */
#define PG_PAGESIZE ((1ULL << 7))

/*
 * Global page
 */
#define PG_GLOBAL ((1ULL << 8))

/*
 * PAT bit (for large pages)
 */
#define PG_PAT_LARGE ((1ULL << 12))

/*
 * PAT bit (for PTEs)
 */
#define PG_PAT_PTE ((1ULL << 7))

/*
 * Page COW attribute (STAGE3-specific)
 */
#define PG_COPY_ON_WRITE ((1ULL << 6))

/*
 * x86_64 does not have a "READ" or "EXEC" bit, it's implied
 * (the latter by the lack of NOEXEC).
 *
 * Just define zero so they have no effect.
 */
#define PG_READ ((0))
#define PG_EXEC ((0))

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

#define IS_USER_ADDRESS(ptr) (((((uint64_t)(ptr)) & 0xffff800000000000) == 0))
#define IS_PAGE_ALIGNED(ptr) ((((uint64_t)(ptr)) & PAGE_RELATIVE_MASK) == 0)

#define PAGE_TABLE_ENTRIES ((512))

#ifdef UNIT_TESTS
#ifndef MUNIT_H
extern
#endif
        uint8_t mock_cpu_temp_page[0x1000];
#endif

typedef struct {
    uint64_t entries[512];
} PageTable;

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
uintptr_t vmm_per_cpu_temp_page_addr(const uint8_t cpu);

uintptr_t vmm_phys_to_virt(const uintptr_t phys_addr);

void *vmm_phys_to_virt_ptr(const uintptr_t phys_addr);

uintptr_t vmm_virt_to_phys_page(const uintptr_t virt_addr);

PageTable *vmm_find_pml4();

uint16_t vmm_virt_to_table_index(const uintptr_t virt_addr,
                                 const uint8_t level);

uint16_t vmm_virt_to_pml4_index(const uintptr_t virt_addr);

uint16_t vmm_virt_to_pdpt_index(const uintptr_t virt_addr);

uint16_t vmm_virt_to_pd_index(const uintptr_t virt_addr);

uint16_t vmm_virt_to_pt_index(const uintptr_t virt_addr);

uintptr_t vmm_table_entry_to_phys(const uintptr_t table_entry);

uint16_t vmm_table_entry_to_page_flags(const uintptr_t table_entry);

uint64_t vmm_phys_and_flags_to_table_entry(const uintptr_t phys,
                                           const uint64_t flags);

// Convert virtual address to phys via table walk
uintptr_t vmm_virt_to_phys(const uintptr_t virt_addr);

size_t vmm_level_page_size(const uint8_t level);

// Initialize the direct mapping for physical memory
// This must be called during early boot, before SMP
// or userspace is up (since it abuses both those things)
void vmm_init_direct_mapping(uint64_t *pml4, const Limine_MemMap *memmap);

#endif //__ANOS_KERNEL_ARCH_X86_64_VM_MAPPER_H
