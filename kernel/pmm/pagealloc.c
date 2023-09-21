/*
 * stage3 - The page allocator (Modified stack allocator)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>

#include "machine.h"
#include "pmm/pagealloc.h"

MemoryRegion* page_alloc_init(E820h_MemMap *memmap, void* buffer) {
    MemoryRegion *region = (MemoryRegion*)buffer;
    region->sp = (MemoryBlock*)(region + 1);
    region->sp--;   // Start below bottom of stack

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
            region->sp->size = total_bytes >> 12;   // size is pages, not bytes...
        }
    }

    return region;
}

uint64_t page_alloc(MemoryRegion *region) {
    if (region->sp < ((MemoryBlock*)(region + 1))) {
        // Empty stack
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

static inline bool stack_empty(MemoryRegion *region) {
    return region->sp == (((MemoryBlock*)(region + 1)) - 1);
}

void page_free(MemoryRegion *region, uint64_t page) {
    // No-op unaligned addresses...
    if (page & 0xFFF) {
        return;
    }

    region->free += 0x1000;

#   ifndef NO_PMM_FREE_COALESCE_ADJACENT
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
#   endif

    // Freeing non-contiguous page, just stack
    region->sp++;
    region->sp->base = page;
    region->sp->size = 1;
}