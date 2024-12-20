/*
 * stage3 - The page allocator (Modified stack allocator)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>

#include "machine.h"
#include "pmm/pagealloc.h"

#ifdef HOSTED_PMM_PRINTF_DEBUGGING
#include <stdio.h>
#define hprintf(...) printf(__VA_ARGS__)
#else
#define hprintf(...)
#endif

MemoryRegion *page_alloc_init(E820h_MemMap *memmap, uint64_t managed_base,
                              void *buffer) {
    MemoryRegion *region = (MemoryRegion *)buffer;
    region->sp = (MemoryBlock *)(region + 1);
    region->sp--; // Start below bottom of stack

    region->size = region->free = 0;

    for (int i = 0; i < memmap->num_entries; i++) {
        E820h_MemMapEntry *entry = &memmap->entries[i];

        if (entry->type == MEM_MAP_ENTRY_AVAILABLE && entry->length > 0) {
            // Ensure start is page aligned
            uint64_t start = entry->base & 0xFFFFFFFFFFFFF000;

            // Grab end, and align that too
            uint64_t end = (entry->base + entry->length) & 0xFFFFFFFFFFFFF000;

            // Is the aligned base below the actual base for this block?
            if (entry->base > start) {
                // Round up to next page boundary if so...
                start += 0x1000;
            }

            // Cut off any memory below the supplied managed base.
            if (start < managed_base) {
                if (end <= managed_base) {
                    // This block is entirely below the managed base, just skip
                    // it
                    continue;
                } else {
                    // This block extends beyond the managed base, so adjust the
                    // start
                    start = managed_base;
                }
            }

            // Calculate number of bytes remaining...
            uint64_t total_bytes = end - start;

            // Just in case we get a block < 4KiB
            if (total_bytes == 0) {
                continue;
            }

            region->size += total_bytes;
            region->free += total_bytes;

            // Stack this block
            region->sp++;
            region->sp->base = start;
            region->sp->size = total_bytes >> 12; // size is pages, not bytes...
        }
    }

    return region;
}

static inline bool stack_empty(MemoryRegion *region) {
    return region->sp < ((MemoryBlock *)(region + 1));
}

uint64_t page_alloc_m(MemoryRegion *region, uint64_t count) {
    if (stack_empty(region)) {
        return 0xFF;
    }

    MemoryBlock *ptr = region->sp;

    hprintf("\n\nBlock: %p\n", region);
    while (ptr >= ((MemoryBlock *)(region + 1))) {
        hprintf("Check block %p - 0x%016x : 0x%016x\n", ptr, ptr->base,
                ptr->size);
        if (ptr->size > count) {
            // Block is more than enough, just split it and return
            uint64_t page = ptr->base;

            hprintf("  Split block and allocate 0x%016x\n", page);
            ptr->base += 0x1000;
            ptr->size -= count;

            region->free -= (count << 12);
            return page;
        } else if (ptr->size == count) {
            // Block is exactly enough, pop (or remove if not top) it and return
            uint64_t page = ptr->base;
            hprintf("  Remove block and allocate 0x%016x\n", page);

            if (ptr != region->sp) {
                // it's not the top block, replace this with the region
                // from the top of the stack...
                ptr->base = region->sp->base;
                ptr->size = region->sp->size;
            }

            // ... now pop the top either way since we've either used it,
            // or moved it to replace the one we removed.
            region->sp--;

            region->free -= (count << 12);
            return page;
        }

        ptr--;
    }

    return 0xFF;
}

uint64_t page_alloc(MemoryRegion *region) {
    if (stack_empty(region)) {
        return 0xFF;
    }

    region->free -= 0x1000;

    if (region->sp->size > 1) {
        // More than one page in this block - just adjust in-place
        uint64_t page = region->sp->base;
        region->sp->base += 0x1000;
        region->sp->size--;
        return page;
    } else {
        // Must be exactly one page in this block - just pop and return
        uint64_t page = region->sp->base;
        region->sp--;
        return page;
    }
}

void page_free(MemoryRegion *region, uint64_t page) {
    // No-op unaligned addresses...
    if (page & 0xFFF) {
        return;
    }

    region->free += 0x1000;

#ifndef NO_PMM_FREE_COALESCE_ADJACENT
    if (!stack_empty(region)) {
        if (region->sp->base == page + 0x1000) {
            // Freeing page below current stack top, so just rebase and resize
            region->sp->base = page;
            region->sp->size += 1;
            return;
        } else if (region->sp->base == page - 0x1000) {
            // Freeing page above current stack top, so just resize
            region->sp->size += 1;
            return;
        }
    }
#endif

    // Freeing non-contiguous page, just stack
    region->sp++;
    region->sp->base = page;
    region->sp->size = 1;
}