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

#ifndef __ANOS_KERNEL_VM_MAPPER_H
#define __ANOS_KERNEL_VM_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vmm/vmconfig.h"

typedef enum {
    PT_LEVEL_PML4 = 4,
    PT_LEVEL_PDPT = 3,
    PT_LEVEL_PD = 2,
    PT_LEVEL_PT = 1,
} PagetableLevel;

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

#ifdef UNIT_TESTS
// In unit tests, we need to leave addresses alone...
static constexpr uintptr_t DIRECT_MAP_BASE = 0;
#else
// Direct mapping base address (from MemoryMap.md)
static constexpr uintptr_t DIRECT_MAP_BASE = 0xffff800000000000;
#endif

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
 * with the specified page tables.
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
