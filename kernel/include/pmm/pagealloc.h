/*
 * stage3 - The page allocator
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PMM_PAGEALLOC_H
#define __ANOS_KERNEL_PMM_PAGEALLOC_H

#include <stdbool.h>

#include "machine.h"

#define MAX_ORDER           10                              // Max order is 10 = 4MiB

typedef struct {
    uintptr_t       phys_addr;
} PhysPage;

typedef struct _PhysicalBlock {
    uintptr_t                   base;
    uintptr_t                   order;
    struct _PhysicalBlock*      next;
} PhysicalBlock;

typedef struct {
    uint64_t        size;
    uint64_t        free;
    PhysicalBlock*  order_lists[MAX_ORDER];
    uint64_t*       bitmap;
} MemoryRegion;

/*
 * Initialize the allocator.
 * 
 * The parameter is a memory map, as passed in from the bootloader.
 * It will be cleaned up during initialisation, reserved areas will
 * be ignored, overlapping areas will be resolved etc.
 * 
 * For now, ACPI areas will just be ignored.
 * 
 * Will also make sure all free areas are page-aligned.
 * 
 * Returns `true` if initialization succeeded, or `false` otherwise
 * (probably because the map was empty or had no usable memory).
 * 
 * Allocator information is populated into the given PageAllocator
 * struct.
 */
MemoryRegion* page_alloc_init(E820h_MemMap *memmap, void* buffer);

/*
 * Allocate a physical page. 
 *
 * Currently, only 4KiB pages are supported.
 * 
 * Returns `true` if allocation succeeded, or `false` otherwise
 * (probably because there were no free pages left).
 * 
 * Page information (needed for free) is filled into the provided
 * PhysPage structure.
 */
bool page_alloc_alloc_page(PhysPage *page);

/*
 * Free a physical page. 
 *
 * Currently, only 4KiB pages are supported.
 * 
 * Returns `true` if allocation succeeded, or `false` otherwise
 * (probably because there were no free pages left).
 */
bool page_alloc_free_page(PhysPage *page);

#endif//__ANOS_KERNEL_PMM_PAGEALLOC_H
