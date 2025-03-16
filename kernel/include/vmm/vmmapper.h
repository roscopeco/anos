/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_VM_MAPPER_H
#define __ANOS_KERNEL_VM_MAPPER_H

#include <stdbool.h>
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
#define PRESENT ((1 << 0))

/*
 * Page writeable attribute
 */
#define WRITE ((1 << 1))

/*
 * Page user attribute
 */
#define USER ((1 << 2))

/*
 * Page COW attribute (STAGE3-specific)
 */
#define COPY_ON_WRITE ((1 << 6))

/*
 * Page size attribute (for large pages)
 */
#define PAGESIZE ((1 << 7))

// Again, for now, all physical memory used must be mapped
// here, the mapper expects to be able to access pages
// under this...
#define STATIC_KERNEL_SPACE ((0xFFFFFFFF80000000))

// Just used to page-align addresses to their containing page
#define PAGE_ALIGN_MASK ((0xFFFFFFFFFFFFF000))

// Just used to extract page-relative addresses from their containing page
#define PAGE_RELATIVE_MASK (~PAGE_ALIGN_MASK)

// Just used to extract PTE flags
#define PAGE_FLAGS_MASK PAGE_RELATIVE_MASK

// Base of the per-CPU temporary mapping pages
#define PER_CPU_TEMP_PAGE_BASE ((0xFFFFFFFF80400000))

/*
 *  Find the per-CPU temporary page base for the given CPU.
 */
static inline uintptr_t vmm_per_cpu_temp_page_addr(uint8_t cpu) {
    return PER_CPU_TEMP_PAGE_BASE + (cpu << 12);
}

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
bool vmm_map_page_in(void *pml4, uintptr_t virt_addr, uint64_t page,
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
