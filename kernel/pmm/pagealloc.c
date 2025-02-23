/*
 * stage3 - The page allocator (Modified stack allocator)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>

#include "machine.h"
#include "pmm/pagealloc.h"
#include "spinlock.h"

#ifdef HOSTED_PMM_PRINTF_DEBUGGING
#include <stdio.h>
#define hprintf(...) printf(__VA_ARGS__)
#else
#define hprintf(...)
#endif

#ifdef DEBUG_PMM
#include "debugprint.h"
#include "printdec.h"
#include "printhex.h"

#define C_DEBUGSTR debugstr
#define C_PRINTHEX64 printhex64
#define C_PRINTDEC printdec
#ifdef VERY_NOISY_PMM
#define V_DEBUGSTR debugstr
#define V_PRINTHEX64 printhex64
#define V_PRINTDEC printdec
#else
#define V_DEBUGSTR(...)
#define V_PRINTHEX64(...)
#define V_PRINTDEC(...)
#endif
#else
#define C_DEBUGSTR(...)
#define C_PRINTHEX64(...)
#define C_PRINTDEC(...)
#define V_DEBUGSTR(...)
#define V_PRINTHEX64(...)
#define V_PRINTDEC(...)
#endif

MemoryRegion *page_alloc_init_e820(E820h_MemMap *memmap, uint64_t managed_base,
                                   void *buffer) {
    MemoryRegion *region = (MemoryRegion *)buffer;
    spinlock_init(&region->lock);
    region->sp = (MemoryBlock *)(region + 1);
    region->sp--; // Start below bottom of stack

    region->size = region->free = 0;

    C_DEBUGSTR("PMM Managed base: ");
    C_PRINTDEC(managed_base, debugchar);
    C_DEBUGSTR("\n");

    for (int i = 0; i < memmap->num_entries; i++) {
        E820h_MemMapEntry *entry = &memmap->entries[i];

        if (entry->type == MEM_MAP_ENTRY_AVAILABLE && entry->length > 0) {
            C_DEBUGSTR(" ====> Mapping available region ");
            C_PRINTHEX64(entry->base, debugchar);
            C_DEBUGSTR(" of length ");
            C_PRINTDEC(entry->length, debugchar);
            C_DEBUGSTR(" [type ");
            C_PRINTDEC(entry->type, debugchar);
            C_DEBUGSTR("]\n");

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
                    C_DEBUGSTR(" ==== ----> Ignoring, entirely below base\n");
                    continue;
                } else {
                    // This block extends beyond the managed base, so adjust the
                    // start
                    C_DEBUGSTR(" ==== ----> Adjusting, partially below base\n");
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
        } else {
            C_DEBUGSTR(" ====> Skipping unavailable region ");
            C_PRINTHEX64(entry->base, debugchar);
            C_DEBUGSTR(" of length ");
            C_PRINTDEC(entry->length, debugchar);
            C_DEBUGSTR(" [type ");
            C_PRINTDEC(entry->type, debugchar);
            C_DEBUGSTR("]\n");
        }
    }

    return region;
}

MemoryRegion *page_alloc_init_limine(Limine_MemMap *memmap,
                                     uint64_t managed_base, void *buffer) {
    MemoryRegion *region = (MemoryRegion *)buffer;
    spinlock_init(&region->lock);
    region->sp = (MemoryBlock *)(region + 1);
    region->sp--; // Start below bottom of stack

    region->size = region->free = 0;

    C_DEBUGSTR("PMM Managed base: ");
    C_PRINTDEC(managed_base, debugchar);
    C_DEBUGSTR("\n");

    for (int i = 0; i < memmap->entry_count; i++) {
        Limine_MemMapEntry *entry = memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length > 0) {
            C_DEBUGSTR(" ====> Mapping available region ");
            C_PRINTHEX64(entry->base, debugchar);
            C_DEBUGSTR(" of length ");
            C_PRINTDEC(entry->length, debugchar);
            C_DEBUGSTR(" [type ");
            C_PRINTDEC(entry->type, debugchar);
            C_DEBUGSTR("]\n");

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
                    C_DEBUGSTR(" ==== ----> Ignoring, entirely below base\n");
                    continue;
                } else {
                    // This block extends beyond the managed base, so adjust the
                    // start
                    C_DEBUGSTR(" ==== ----> Adjusting, partially below base\n");
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
        } else {
            C_DEBUGSTR(" ====> Skipping unavailable region ");
            C_PRINTHEX64(entry->base, debugchar);
            C_DEBUGSTR(" of length ");
            C_PRINTDEC(entry->length, debugchar);
            C_DEBUGSTR(" [type ");
            C_PRINTDEC(entry->type, debugchar);
            C_DEBUGSTR("]\n");
        }
    }

    return region;
}

static inline bool stack_empty(MemoryRegion *region) {
    return region->sp < ((MemoryBlock *)(region + 1));
}

uint64_t page_alloc_m(MemoryRegion *region, uint64_t count) {
    spinlock_lock(&region->lock);

    if (stack_empty(region)) {
        spinlock_unlock(&region->lock);
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

            spinlock_unlock(&region->lock);
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

            spinlock_unlock(&region->lock);
            return page;
        }

        ptr--;
    }

    spinlock_unlock(&region->lock);
    return 0xFF;
}

uint64_t page_alloc(MemoryRegion *region) {
    spinlock_lock(&region->lock);

    if (stack_empty(region)) {
        spinlock_unlock(&region->lock);
        return 0xFF;
    }

    region->free -= 0x1000;

    if (region->sp->size > 1) {
        // More than one page in this block - just adjust in-place
        uint64_t page = region->sp->base;
        region->sp->base += 0x1000;
        region->sp->size--;

        spinlock_unlock(&region->lock);
        return page;
    } else {
        // Must be exactly one page in this block - just pop and return
        uint64_t page = region->sp->base;
        region->sp--;

        spinlock_unlock(&region->lock);
        return page;
    }
}

void page_free(MemoryRegion *region, uint64_t page) {
    // No-op unaligned addresses...
    if (page & 0xFFF) {
        return;
    }

    spinlock_lock(&region->lock);
    region->free += 0x1000;

#ifndef NO_PMM_FREE_COALESCE_ADJACENT
    if (!stack_empty(region)) {
        if (region->sp->base == page + 0x1000) {
            // Freeing page below current stack top, so just rebase and resize
            region->sp->base = page;
            region->sp->size += 1;

            spinlock_unlock(&region->lock);
            return;
        } else if (region->sp->base == page - 0x1000) {
            // Freeing page above current stack top, so just resize
            region->sp->size += 1;

            spinlock_unlock(&region->lock);
            return;
        }
    }
#endif

    // Freeing non-contiguous page, just stack
    region->sp++;
    region->sp->base = page;
    region->sp->size = 1;
    spinlock_unlock(&region->lock);
}