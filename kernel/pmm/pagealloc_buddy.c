/*
 * stage3 - The page allocator (Buddy version)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 * 
 * This is a buddy allocator, which isn't ideal as a PMM.
 * It'll get used for VM address-space allocation though.
 */

#include <stdbool.h>

#include "machine.h"
#include "pmm/pagealloc.h"

#define NULL                ((void*)0)

#define PAGE_SIZE           4096                            // 4KiB pages only for now
#define MAX_ORDER_PAGES     1 << (MAX_ORDER-1)              // Max number of pages in a block
#define MAX_ORDER_BYTES     PAGE_SIZE << (MAX_ORDER-1)      // Maximum block size, in bytes

/*
 * Standard popcount algorithm from wikipedia.
 *
 * TODO it's probably safe to use the POPCNT instruction for this,
 * it was introduced in like 2013...
 */
static uint8_t popcnt(uint64_t y) {
    y -= ((y >> 1) & 0x5555555555555555ull);
    y = (y & 0x3333333333333333ull) + (y >> 2 & 0x3333333333333333ull);
    return ((y + (y >> 4)) & 0xf0f0f0f0f0f0f0full) * 0x101010101010101ull >> 56;
}

/*
 * Round **down** to next power of 2, to get largest 
 * possible block size once we're below max.
 */
static uint64_t next_lower_pow2(uint64_t x) {
    if (x == 0) {
        return 0;
    }

    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return x - (x >> 1);
}

// TODO these regions can overlap, be out of order, etc. Need to clean them up!
//
//      clean this function up while you're at it 🤣
MemoryRegion* page_alloc_init(E820h_MemMap *memmap, void* buffer) {

    MemoryRegion *region = (MemoryRegion*)buffer;
    PhysicalBlock *next_block = (PhysicalBlock*)(region + 1);
    PhysicalBlock *order_nexts[MAX_ORDER];

    region->size = region->free = 0;

    for (int i = 0; i < MAX_ORDER; i++) {
        region->order_lists[i] = NULL;
        order_nexts[i] = NULL;
    }

    for (int i = 0; i < memmap->num_entries; i++) {
        E820h_MemMapEntry *entry = &memmap->entries[i];

        if (entry->type == MEM_MAP_ENTRY_AVAILABLE) {
            region->size += entry->length;
            region->free += entry->length;

            // Ensure start is page aligned
            uint64_t start = entry->base & 0xFFFFFFFFFFFFF000;

            // Grab end, and align that too
            uint64_t end = (entry->base + entry->length) &  0xFFFFFFFFFFFFF000;

            // Is the aligned base below the actual base for this block?
            if (start > entry->base) {
                // Round up to next page boundary if so...
                start += 0x1000;
            }

            // Calculate number of bytes remaining...
            uint64_t bytes_remain = end - start;

            while (bytes_remain > 0) {                          
                if (bytes_remain >= MAX_ORDER_BYTES) {
                    // Just take largest possible page
                    next_block->base = start;
                    next_block->order = MAX_ORDER - 1;
                    next_block->next = NULL;

                    start += MAX_ORDER_BYTES;
                    bytes_remain -= MAX_ORDER_BYTES;
                } else {
                    // Work out the largest page we can take...
                    uint64_t largest_possible = next_lower_pow2(bytes_remain);

                    // Figure out the order (pages are always powers of 2, so pop count in base - 1 is the order)
                    uint8_t order = popcnt((largest_possible >> 12) - 1);

                    next_block->base = start;
                    next_block->order = order;
                    next_block->next = NULL;

                    start += largest_possible;
                    bytes_remain -= largest_possible;
                }

                if (region->order_lists[next_block->order] == NULL) {
                    // This is the first block for this order, so link it from the region
                    region->order_lists[next_block->order] = next_block;
                } else {
                    // This is not the first block for this order, so we must already have a previous
                    // in order_nexts - just link up to that.
                    order_nexts[next_block->order]->next = next_block;
                }

                order_nexts[next_block->order] = next_block;
                next_block++;
            }
        }
    }

    return region;
}

static bool find_split_page(MemoryRegion *region, uint8_t order) {
    
}

bool page_alloc_alloc_page(MemoryRegion *region, uint8_t order, PhysPage *page) {
    
    return false;
}
