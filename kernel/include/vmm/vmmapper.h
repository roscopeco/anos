/*
 * stage3 - The virtual-memory mapper
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_VM_MAPPER_H
#define __ANOS_KERNEL_VM_MAPPER_H

#include <stdint.h>

/*
 * Extract a PML4 index from a virtual address.
 */
#define PML4ENTRY(addr)  (( (unsigned short)((addr & 0x0000ff8000000000) >> 39) ))

/*
 * Extract a PDPT index from a virtual address.
 */
#define PDPTENTRY(addr)  (( (unsigned short)((addr & 0x0000007fc0000000) >> 30) ))

/*
 * Extract a PD index from a virtual address.
 */
#define PDENTRY(addr)    (( (unsigned short)((addr & 0x000000003fe00000) >> 21) ))

/*
 * Extract a PT index from a virtual address.
 */
#define PTENTRY(addr)    (( (unsigned short)((addr & 0x00000000001ff000) >> 12) ))

/*
 * Page present attribute
 */
#define PRESENT     (1 << 0)

/*
 * Page writeable attribute
 */
#define WRITE       (1 << 1)

// For now, just supporting a single set of page tables,
// mapped into kernel space at this address.
#define STATIC_PML4            ((uint64_t*) 0xFFFFFFFF8009c000)

// Again, for now, all physical memory used must be mapped
// here, the mapper expects to be able to access pages
// under this...
#define STATIC_KERNEL_SPACE     0xFFFFFFFF80000000

// Just used to page-align addresses to their containing page
#define PAGE_ALIGN_MASK         0xFFFFFFFFFFFFF000

/*
 * Map a page into virtual memory. 
 *
 * This always maps under the static PML4 at the address given above.
 * It will create PDPT/PD/PT entries and associated tables as needed,
 * which means it needs to allocate physical pages - it uses the PMM
 * (obviously) and thus it **can** pagefault.
 */
void map_page(uint64_t *pml4, uintptr_t virt_addr, uint32_t page, uint16_t flags);

#endif//__ANOS_KERNEL_VM_MAPPER_H

