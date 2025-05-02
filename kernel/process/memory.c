/*
 * stage3 - Process memory management
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * Aims: 
 *   * No (de)allocation while holding locks.
 *   * Safe with deferred allocation in page fault handler
 *   * Safe even on allocators that use internal mutexes (glibc, jemalloc, slab...).
 *   * Retains correct block compaction and list unlinking.
 *   * Doesn’t require heap memory tracking inside the lock.
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
    if (!proc || !proc->pages_lock)
        return false;

    if (shared && refcount_map_increment(phys_addr) == 0)
        return false;

    ProcessPages *new_pages = NULL;
    ProcessPageBlock *new_blk = NULL;

    if (!proc->pages) {
        new_pages = fba_alloc_block();
        if (!new_pages) {
            if (shared)
                refcount_map_decrement(phys_addr);
            return false;
        }
        new_pages->head = NULL;
    }

    spinlock_lock_irqsave(proc->pages_lock);

    if (!proc->pages) {
        proc->pages = new_pages;
        new_pages = NULL;
    }

    ProcessPageBlock *blk = proc->pages->head;
    while (blk && blk->count >= PAGES_PER_BLOCK)
        blk = blk->next;

    if (blk) {
        blk->pages[blk->count++] =
                (ProcessPageEntry){.addr = phys_addr, .region = region};
        spinlock_unlock_irqrestore(proc->pages_lock, 0);
        if (new_pages)
            fba_free(new_pages);
        return true;
    }

    spinlock_unlock_irqrestore(proc->pages_lock, 0);

    new_blk = fba_alloc_block();
    if (!new_blk) {
        if (shared)
            refcount_map_decrement(phys_addr);
        if (new_pages)
            fba_free(new_pages);
        return false;
    }
    new_blk->count = 0;
    new_blk->next = NULL;

    spinlock_lock_irqsave(proc->pages_lock);

    blk = proc->pages->head;
    while (blk && blk->count >= PAGES_PER_BLOCK)
        blk = blk->next;

    if (blk) {
        blk->pages[blk->count++] =
                (ProcessPageEntry){.addr = phys_addr, .region = region};
        spinlock_unlock_irqrestore(proc->pages_lock, 0);
        fba_free(new_blk);
        if (new_pages)
            fba_free(new_pages);
        return true;
    }

    new_blk->next = proc->pages->head;
    proc->pages->head = new_blk;
    new_blk->pages[new_blk->count++] =
            (ProcessPageEntry){.addr = phys_addr, .region = region};

    spinlock_unlock_irqrestore(proc->pages_lock, 0);
    if (new_pages)
        fba_free(new_pages);
    return true;
}

bool process_remove_owned_page(Process *proc, uintptr_t phys_addr) {
    if (!proc || !proc->pages || !proc->pages_lock)
        return false;

    ProcessPageBlock *prev = NULL;
    ProcessPageBlock *blk = NULL;
    ProcessPageEntry removed_entry = {0};
    ProcessPageBlock *block_to_free = NULL;
    bool found = false;

    spinlock_lock_irqsave(proc->pages_lock);

    blk = proc->pages->head;
    while (blk) {
        for (uint16_t i = 0; i < blk->count; ++i) {
            if (blk->pages[i].addr == phys_addr) {
                removed_entry = blk->pages[i];
                found = true;
                blk->pages[i] = blk->pages[--blk->count];

                if (blk->count == 0) {
                    if (prev) {
                        prev->next = blk->next;
                    } else {
                        proc->pages->head = blk->next;
                    }
                    block_to_free = blk;
                }
                goto done;
            }
        }
        prev = blk;
        blk = blk->next;
    }

done:
    spinlock_unlock_irqrestore(proc->pages_lock, 0);

    if (found) {
        uint32_t prev = refcount_map_decrement(removed_entry.addr);
        if (prev <= 1) {
            page_free(removed_entry.region, removed_entry.addr);
        }

        if (block_to_free) {
            fba_free(block_to_free);
        }
    }

    return found;
}

void process_release_owned_pages(Process *proc) {
    if (!proc || !proc->pages_lock)
        return;

    ProcessPageBlock *blk = NULL;

    spinlock_lock_irqsave(proc->pages_lock);

    if (!proc->pages) {
        spinlock_unlock_irqrestore(proc->pages_lock, 0);
        return;
    }

    blk = proc->pages->head;
    proc->pages->head = NULL;
    fba_free(proc->pages);
    proc->pages = NULL;

    spinlock_unlock_irqrestore(proc->pages_lock, 0);

    while (blk) {
        for (uint16_t i = 0; i < blk->count; ++i) {
            uintptr_t addr = blk->pages[i].addr;
            MemoryRegion *region = blk->pages[i].region;
            uint32_t prev = refcount_map_decrement(addr);
            if (prev <= 1) {
                page_free(region, addr);
            }
        }
        ProcessPageBlock *next = blk->next;
        fba_free(blk);
        blk = next;
    }
}

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
