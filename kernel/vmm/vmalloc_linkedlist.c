/*
 * stage3 - The virtual address space allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * This is a simple linked-list based allocator with coalescing on free.
 * It'll do the job for now...
 */

#include "once.h"
#include "spinlock.h"
#include "vmm/vmalloc.h"

#define NULL (((void *)0))

static SpinLock vmm_lock;

// A range represents either a free or allocated block of virtual address space
typedef struct Range {
    uint64_t start;     // Start address of range
    uint64_t size;      // Size of range in bytes
    struct Range *next; // Next range in list, sorted by address
} Range;

// State management
static Range *free_ranges;    // List of free ranges, sorted by address
static Range *range_pool;     // Pool of unused range structures
static Range *range_pool_end; // End of our metadata region
static bool is_initialized = false;

// Helper: Initialize a range structure
static void init_range(Range *r, uint64_t start, uint64_t size) {
    r->start = start;
    r->size = size;
    r->next = NULL;
}

// Helper: Get a new range structure from the pool
static Range *alloc_range(void) {
    if (!range_pool) {
        return NULL;
    }
    Range *r = range_pool;
    range_pool = range_pool->next;
    return r;
}

// Helper: Return a range structure to the pool
static void free_range(Range *r) {
    r->next = range_pool;
    range_pool = r;
}

// Helper: Align address up to page boundary
static uint64_t align_up(uint64_t addr) {
    return (addr + VM_PAGE_SIZE - 1) & ~((uint64_t)VM_PAGE_SIZE - 1);
}

// Helper: Align address down to page boundary
static uint64_t align_down(uint64_t addr) {
    return addr & ~((uint64_t)VM_PAGE_SIZE - 1);
}

// Initialize the VMM
int vmm_init(void *metadata_start, uint64_t metadata_size,
             uint64_t managed_start, uint64_t managed_size) {

    kernel_guard_once();

    // Basic parameter validation
    if (!metadata_start || !metadata_size || !managed_size) {
        return VMM_ERROR_INVALID_PARAMS;
    }

    // Align managed range to page boundaries
    uint64_t aligned_start = align_up(managed_start);
    uint64_t aligned_end = align_down(managed_start + managed_size);
    if (aligned_end <= aligned_start) {
        return VMM_ERROR_INVALID_PARAMS;
    }

    spinlock_init(&vmm_lock);

    // Initialize the range pool
    range_pool = (Range *)metadata_start;
    range_pool_end = (Range *)((char *)metadata_start + metadata_size);

    // Initialize all range structures in the pool
    Range *curr = range_pool;
    while ((curr + 1) <= range_pool_end) {
        curr->next = curr + 1;
        curr = curr + 1;
    }
    curr--; // Move back to last valid range
    curr->next = NULL;

    // Initialize free ranges list with entire managed range
    free_ranges = alloc_range();
    if (!free_ranges) {
        return VMM_ERROR_NO_SPACE;
    }

    init_range(free_ranges, aligned_start, aligned_end - aligned_start);

    is_initialized = true;
    return VMM_SUCCESS;
}

// Allocate a block of pages
uint64_t vmm_alloc_block(uint64_t num_pages) {
    if (!is_initialized || num_pages == 0) {
        return 0;
    }

    uint64_t lock_flags = spinlock_lock_irqsave(&vmm_lock);

    uint64_t size = num_pages * VM_PAGE_SIZE;
    Range *prev = NULL;
    Range *curr = free_ranges;

    // Find best fit
    Range *best_fit = NULL;
    Range *best_fit_prev = NULL;
    uint64_t smallest_viable_size = (uint64_t)-1;

    while (curr) {
        if (curr->size >= size && curr->size < smallest_viable_size) {
            best_fit = curr;
            best_fit_prev = prev;
            smallest_viable_size = curr->size;
            // Early exit if we find an exact match
            if (curr->size == size) {
                break;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    if (best_fit) {
        uint64_t alloc_addr = best_fit->start;

        // If exact size, remove the range
        if (best_fit->size == size) {
            if (best_fit_prev) {
                best_fit_prev->next = best_fit->next;
            } else {
                free_ranges = best_fit->next;
            }
            free_range(best_fit);
        }
        // Otherwise split the range
        else {
            best_fit->start += size;
            best_fit->size -= size;
        }

        spinlock_unlock_irqrestore(&vmm_lock, lock_flags);
        return alloc_addr;
    }

    spinlock_unlock_irqrestore(&vmm_lock, lock_flags);
    return 0; // No suitable range found
}

// Free a block of pages
int vmm_free_block(uint64_t address, uint64_t num_pages) {
    if (!is_initialized) {
        return VMM_ERROR_NOT_INITIALIZED;
    }
    if (!num_pages || (address & (VM_PAGE_SIZE - 1))) {
        return VMM_ERROR_INVALID_PARAMS;
    }

    uint64_t lock_flags = spinlock_lock_irqsave(&vmm_lock);

    uint64_t size = num_pages * VM_PAGE_SIZE;
    Range *new_range = alloc_range();
    if (!new_range) {
        return VMM_ERROR_NO_SPACE;
    }

    init_range(new_range, address, size);

    // Insert into free list, maintaining address order
    Range *prev = NULL;
    Range *curr = free_ranges;

    // Find insertion point
    while (curr && curr->start < address) {
        prev = curr;
        curr = curr->next;
    }

    // Insert the new range
    if (prev) {
        new_range->next = prev->next;
        prev->next = new_range;
    } else {
        new_range->next = free_ranges;
        free_ranges = new_range;
    }

    // Coalesce with previous range if adjacent
    if (prev && prev->start + prev->size == new_range->start) {
        prev->size += new_range->size;
        prev->next = new_range->next;
        free_range(new_range);
        new_range = prev;
    }

    // Coalesce with next range if adjacent
    if (new_range->next &&
        new_range->start + new_range->size == new_range->next->start) {
        new_range->size += new_range->next->size;
        Range *to_free = new_range->next;
        new_range->next = to_free->next;
        free_range(to_free);
    }

    spinlock_unlock_irqrestore(&vmm_lock, lock_flags);
    return VMM_SUCCESS;
}