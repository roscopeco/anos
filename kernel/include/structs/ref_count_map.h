/*
 * stage3 - Hashtable for shared phys mem tracking
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * A specialized hash map optimized for the use case of tracking
 * shared memory blocks and maintaining their refcounts. 
 * 
 * Key features:
 * 
 * 1. Amortized constant-time operations for all operations:
 * 
 *  Insertion (incrementing reference count)
 *  Lookup (checking reference count)
 *  Deletion (decrementing reference count)
 * 
 * 2. Space efficiency:
 * 
 *  Each entry stores the physical address, reference count, and minimal metadata
 *  Uses chaining for collision resolution but with optimized hash function for physical addresses
 *  Automatic resizing when load factor exceeds threshold
 * 
 * 3. Memory safety:
 * 
 *  Proper cleanup of resources
 *  Handles allocation failures gracefully
 *  Thread-safety can be added with minimal modifications if needed
 * 
 * 4. Optimizations:
 * 
 *  Uses multiply-shift hashing (good with memory address keys)
 *  Inline hash function for performance
 *  Efficient handling of deleted entries
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_REF_COUNT_MAP_H
#define __ANOS_KERNEL_REF_COUNT_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "anos_assert.h"

typedef struct Entry {
    uintptr_t physical_addr;
    uint64_t ref_count;
    bool is_occupied;
    struct Entry *next;
    uint64_t reserved[4];
} Entry;

typedef struct BlockNode {
    void *block;
    struct BlockNode *next;
    uint64_t used;
    uint64_t reserved[5];
} BlockNode;

typedef struct {
    Entry **buckets;
    uint64_t size;
    uint64_t num_entries;
    BlockNode *block_list;
    uint64_t reserved[4];
} RefCountMap;

static_assert_sizeof(Entry, ==, 64);
static_assert_sizeof(BlockNode, ==, 64);
static_assert_sizeof(RefCountMap, ==, 64);

/*
 * Initialize - must be called before other routines are used!
 * (entrypoint handles that...)
 */
bool refcount_map_init(void);

/*
 * Increment the reference count for the given address.
 * 
 * Returns the new reference count for that address, or 0 on error.
 */
uint32_t refcount_map_increment(uintptr_t addr);

/*
 * Decrement the reference count for the given address.
 * 
 * Returns the **previous** reference count for that address,
 * or 0 if no reference count (or error).
 */
uint32_t refcount_map_decrement(uintptr_t addr);

#endif //__ANOS_KERNEL_REF_COUNT_MAP_H
