/*
 * stage3 - Process memory management
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 * 
 * NOTE! These all lock internally - not reentrant!
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PROCESS_MEMORY_H
#define __ANOS_KERNEL_PROCESS_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#include "pmm/pagealloc.h"

typedef struct Process Process;

typedef struct {
    MemoryRegion *region;
    uint64_t addr;
} ProcessPageEntry;

typedef struct ProcessPageBlock {
    struct ProcessPageBlock *next;
    uint16_t count;
    ProcessPageEntry pages[];
} ProcessPageBlock;

typedef struct {
    ProcessPageBlock *head;
} ProcessPages;

/*
 * Add a page to the given Process' list
 * of owned memory pages.
 * 
 * This will rarely be called directly - 
 * `process_page_alloc` should be used instead.
 * 
 * Locking is handled internally!
 */
bool process_add_owned_page(Process *proc, MemoryRegion *region,
                            uintptr_t phys_addr, bool shared);

/*
 * Remove a page from the given Process' list
 * of owned memory.
 * 
 * This will rarely be called directly - 
 * `process_page_free` should be used instead.
 * 
 * Locking is handled internally!
 */
bool process_remove_owned_page(Process *proc, uintptr_t phys_addr);

/*
 * Release all memory owned by the process.
 *
 * This should be called as part of process
 * clean-up.
 * 
 * Locking is handled internally!
 */
void process_release_owned_pages(Process *proc);

/*
 * Allocate a page of process-owned memory for
 * the given process.
 * 
 * Returns a page-aligned address on success, 
 * or non-aligned (at least 0xff) on failure.
 */
uintptr_t process_page_alloc(Process *proc, MemoryRegion *region);

/*
 * Free the given process-owned memory page.
 */
bool process_page_free(Process *proc, uintptr_t phys_addr);

#endif //__ANOS_KERNEL_PROCESS_MEMORY_H