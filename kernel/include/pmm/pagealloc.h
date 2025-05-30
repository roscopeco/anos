/*
 * stage3 - The page allocator
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PMM_PAGEALLOC_H
#define __ANOS_KERNEL_PMM_PAGEALLOC_H

#include <stdbool.h>

#include "machine.h"
#include "spinlock.h"

typedef struct {
    uintptr_t phys_addr;
} PhysPage;

typedef struct {
    uint64_t base;
    uint64_t size;
} MemoryBlock;

typedef struct {
    SpinLock lock;
    uint64_t flags;
    uint64_t size;
    uint64_t free;
    MemoryBlock *sp;
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
 * The supplied buffer will be used for the MemoryRegion struct,
 * as well as the stack of MemoryBlocks, which will grow upward
 * as needed, and must be able to accomodate a fully-fragmented
 * region. Growing upward means, if the buffer is in a virtual
 * alloc area, physical memory will only be allocated as it grows.
 *
 * Any memory found in the memory map that falls below the supplied
 * managed base address will be ignored by the allocator.
 *
 * Returns a MemoryRegion pointer, created in the given buffer.
 */
MemoryRegion *page_alloc_init_limine(Limine_MemMap *memmap,
                                     uint64_t managed_base, void *buffer,
                                     bool reclaim_exec_mods);

/*
 * Allocate a contiguous block of `count` physical pages.
 *
 * Currently, only 4KiB pages are supported.
 *
 * Returns a page aligned start address on success.
 *
 * If unsuccessful, an unaligned number (with 0xFF in the least-significant
 * byte) will be returned.
 *
 * This will get harder to satisfy as memory gets fragmented (obviously)
 * but will only generally be used for memory-mapped buffers for devices,
 * so will likely be used mostly early in kernel start-up...
 */
uintptr_t page_alloc_m(MemoryRegion *region, uint64_t count);

/*
 * Allocate a single physical page.
 *
 * Currently, only 4KiB pages are supported.
 *
 * Returns a page aligned start address on success.
 *
 * If unsuccessful, an unaligned number (with 0xFF in the least-significant
 * byte) will be returned.
 */
uintptr_t page_alloc(MemoryRegion *region);

/*
 * Free a physical page.
 *
 * Currently, only 4KiB pages are supported.
 */
void page_free(MemoryRegion *region, uintptr_t page);

#endif //__ANOS_KERNEL_PMM_PAGEALLOC_H
