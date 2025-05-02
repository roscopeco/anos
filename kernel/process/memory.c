/*
 * stage3 - Process memory management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "pmm/pagealloc.h"
#include "process.h"
#include "process/memory.h"
#include "structs/ref_count_map.h"

#define PAGES_PER_BLOCK                                                        \
    ((4096 - sizeof(struct ProcessPageBlock *)) / sizeof(ProcessPageEntry))

bool process_add_owned_page(Process *proc, MemoryRegion *region,
                            uintptr_t phys_addr, bool shared) {
    if (!proc) {
        return false;
    }

    uint64_t flags = spinlock_lock_irqsave(proc->pages_lock);

    if (!proc->pages) {
        proc->pages = fba_alloc_block();
        if (!proc->pages) {
            spinlock_unlock_irqrestore(proc->pages_lock, flags);
            return false;
        }
        proc->pages->head = NULL;
    }

    if (shared && refcount_map_increment(phys_addr) == 0) {
        spinlock_unlock_irqrestore(proc->pages_lock, flags);
        return false;
    }

    ProcessPageBlock *blk = proc->pages->head;
    while (blk && blk->count >= PAGES_PER_BLOCK)
        blk = blk->next;

    if (!blk) {
        blk = fba_alloc_block();
        if (!blk) {
            spinlock_unlock_irqrestore(proc->pages_lock, flags);
            return false;
        }
        blk->count = 0;
        blk->next = proc->pages->head;
        proc->pages->head = blk;
    }

    blk->pages[blk->count++] =
            (ProcessPageEntry){.region = region, .addr = phys_addr};
    spinlock_unlock_irqrestore(proc->pages_lock, flags);
    return true;
}

bool process_remove_owned_page(Process *proc, uintptr_t phys_addr) {
    if (!proc || !proc->pages)
        return false;

    uint64_t flags = spinlock_lock_irqsave(proc->pages_lock);

    ProcessPageBlock *blk = proc->pages->head;
    ProcessPageBlock *prev = NULL;

    while (blk) {
        for (uint16_t i = 0; i < blk->count; ++i) {
            if (blk->pages[i].addr == phys_addr) {
                uint32_t prev_ref = refcount_map_decrement(phys_addr);

                if (prev_ref <= 1) {
                    page_free(blk->pages[i].region, phys_addr);
                }

                blk->pages[i] = blk->pages[--blk->count];

                // If block becomes empty, remove it
                if (blk->count == 0) {
                    if (prev) {
                        prev->next = blk->next;
                    } else {
                        proc->pages->head = blk->next;
                    }
                    fba_free(blk);
                }

                spinlock_unlock_irqrestore(proc->pages_lock, flags);
                return true;
            }
        }
        prev = blk;
        blk = blk->next;
    }

    spinlock_unlock_irqrestore(proc->pages_lock, flags);
    return false;
}

void process_release_owned_pages(Process *proc) {
    if (!proc || !proc->pages) {
        return;
    }

    uint64_t flags = spinlock_lock_irqsave(proc->pages_lock);

    ProcessPageBlock *blk = proc->pages->head;
    while (blk) {
        for (uint16_t i = 0; i < blk->count; ++i) {
            uint64_t addr = blk->pages[i].addr;
            void *region = blk->pages[i].region;

            uint32_t prev = refcount_map_decrement(addr);

            if (prev <= 1) {
                page_free(region, addr);
            }
        }
        ProcessPageBlock *next = blk->next;
        fba_free(blk);
        blk = next;
    }

    fba_free(proc->pages);
    proc->pages = NULL;

    spinlock_unlock_irqrestore(proc->pages_lock, flags);
}

// --- Process Memory Allocator API ---

uintptr_t process_page_alloc(Process *proc, MemoryRegion *region) {
    if (!proc) {
        return 0xff;
    }

    uintptr_t addr = page_alloc(region);
    if ((addr & 0xff)) {
        return addr;
    }

    if (!process_add_owned_page(proc, region, addr, false)) {
        page_free(region, addr);
        return 0xff;
    }

    return addr;
}

bool process_page_free(Process *proc, uintptr_t phys_addr) {
    if (!proc) {
        return false;
    }

    return process_remove_owned_page(proc, phys_addr);
}
