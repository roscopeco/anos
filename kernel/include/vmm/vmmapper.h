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

#ifndef __ANOS_KERNEL_VM_MAPPER_H
#define __ANOS_KERNEL_VM_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vmm/vmconfig.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support constexpr yet - Jan 2025
#ifndef constexpr
#define constexpr const
#endif
#endif

// Base of the per-CPU temporary mapping pages
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))

// This is where we map the PMM region(s)
static constexpr uintptr_t STATIC_KERNEL_SPACE = 0xFFFFFFFF80000000;

// Direct mapping base address (from MemoryMap.md)
static constexpr uintptr_t DIRECT_MAP_BASE = 0xffff800000000000;

#define IS_USER_ADDRESS(ptr) ((((uint64_t)ptr & 0xffff800000000000) == 0))

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
static uintptr_t vmm_per_cpu_temp_page_addr(uint8_t cpu);

// Convert direct-mapped virtual address to physical address
static uintptr_t vmm_virt_to_phys(uintptr_t virt_addr);

// Get the table index for a virtual address at a given table level
static uint16_t vmm_virt_to_table_index(uintptr_t virt_addr, uint8_t level);

// Get the PML4 index for a given virtual address
static uint16_t vmm_virt_to_pml4_index(uintptr_t virt_addr);

// Get the PDPT index for a given virtual address
static uint16_t vmm_virt_to_pdpt_index(uintptr_t virt_addr);

// Get the PD index for a given virtual address
static uint16_t vmm_virt_to_pd_index(uintptr_t virt_addr);

// Get the PT index for a given virtual address
static uint16_t vmm_virt_to_pt_index(uintptr_t virt_addr);

// Extract the physical address from a page table entry
static uintptr_t vmm_table_entry_to_phys(uintptr_t table_entry);

// Extract the flags from a page table entry
static uint16_t vmm_table_entry_to_page_flags(uintptr_t table_entry);

// Build a table entry from physical address and flags
static uint64_t vmm_phys_and_flags_to_table_entry(uintptr_t phys,
                                                  uint64_t flags);

// Get the page size for a large page mapped at the given table level
static size_t vmm_level_page_size(uint8_t level);

#if defined __x86_64__
#include "x86_64/vmm/vmmapper.h"
#elif defined __riscv
#include "riscv64/vmm/vmmapper.h"
#else
#error Undefined or unsupported architecture
#endif

/*
 * Map the given page-aligned physical address into virtual memory 
 * with the specified page tables.
 *
 * This will create PDPT/PD/PT entries and associated tables as needed,
 * which means it needs to allocate physical pages - it uses the PMM
 * (obviously) and thus it **can** pagefault.
 *
 * This function invalidates the TLB automatically.
 */
bool vmm_map_page_in(uint64_t *pml4, uintptr_t virt_addr, uint64_t page,
                     uint16_t flags);

/*
 * Map the given page-aligned physical address into virtual memory 
 * with the current page tables.
 *
 * This will create PDPT/PD/PT entries and associated tables as needed,
 * which means it needs to allocate physical pages - it uses the PMM
 * (obviously) and thus it **can** pagefault.
 */
bool vmm_map_page(uintptr_t virt_addr, uint64_t page, uint16_t flags);

/*
 * Map the page containing the given physical address into virtual memory
 * with the current page tables.
 *
 * Simple wrapper around `map_page` - see documentation for that function
 * for specifics.
 */
bool vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr,
                             uint16_t flags);

/*
 * Map the page containing the given physical address into virtual memory
 * with the specified page tables..
 *
 * Simple wrapper around `map_page` - see documentation for that function
 * for specifics.
 */
bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t virt_addr,
                                uint64_t phys_addr, uint16_t flags);

/*
 * Unmap the given virtual page from virtual memory with the current 
 * page tables...
 *
 * This is a "hard" unmap - it will zero out the PTE (rather than, say,
 * setting the page not present) and invalidate the TLB automatically.
 *
 * This function does **not** free any physical memory or otherwise
 * compact the page tables, as doing this on every unmap would be
 * expensive and unnecessary.
 *
 * Returns the physical address that was previously mapped, or
 * 0 for none.
 */
uintptr_t vmm_unmap_page(uintptr_t virt_addr);

/*
 * Unmap the given virtual page from virtual memory with the current 
 * page tables...
 *
 * This is a "hard" unmap - it will zero out the PTE (rather than, say,
 * setting the page not present) and invalidate the TLB automatically.
 *
 * This function does **not** free any physical memory or otherwise
 * compact the page tables, as doing this on every unmap would be
 * expensive and unnecessary.
 *
 * Returns the physical address that was previously mapped, or
 * 0 for none.
 */
uintptr_t vmm_unmap_page_in(uint64_t *pml4, uintptr_t virt_addr);

/*
 * Invalidate the TLB for the page containing the given virtual address.
 *
 * The mapping functions will do this automatically, so it shouldn't be
 * needed most of the time.
 */
void vmm_invalidate_page(uintptr_t virt_addr);

#endif //__ANOS_KERNEL_VM_MAPPER_H
