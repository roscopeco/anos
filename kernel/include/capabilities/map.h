/*
 * Capability Map (Hash Table)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is a dynamically-sized hash map optimized for mapping 64-bit integer
 * capability cookies to pointer values. It uses the FBA for backing memory
 * and IRQ-safe spinlocks to ensure consistent and safe access across cores.
 *
 * Algorithm:
 * - Open addressing hash table with linear probing for collision resolution.
 * - MurmurHash3 finalizer for hashing 64-bit keys.
 * - Lazy deletion using tombstone flags.
 * - Locking via `SpinLock` (IRQ-save/restore) ensures thread/interrupt safety.
 *
 * Performance:
 * - Average-case O(1) lookup, insert, and delete.
 * - Worst-case linear scan, mitigated by load factor and cleanup.
 * - Grows geometrically (capacity × 2) when load exceeds 75%.
 *
 * Memory Usage:
 * - Entries are tightly packed at 24 bytes each.
 * - Memory is allocated in 4KiB blocks; map resizes to keep load < 0.75.
 * - Resizing frees old memory using tracked block count.
 * - Cleanup operation compacts table and reclaims tombstoned slots.
 *
 * Notes:
 * - No shrinking is performed automatically.
 * - We'll need to trigger compaction via `capability_map_cleanup()`. (still TODO)
 * - Fully self-contained, safe for shared use in kernel subsystems. 
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PROCESS_CAPABILITY_MAP_H
#define __ANOS_KERNEL_PROCESS_CAPABILITY_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "anos_assert.h"
#include "spinlock.h"

typedef struct {
    uint64_t key;
    void *value;
    bool occupied;
    bool tombstone; // for lazy deletion
} CapabilityMapEntry;

typedef struct {
    CapabilityMapEntry *entries;
    size_t capacity;
    size_t size;
    size_t block_count;
    SpinLock *lock;
    uint64_t reserved[3];
} CapabilityMap;

static_assert_sizeof(CapabilityMapEntry, ==, 24);
static_assert_sizeof(CapabilityMap, ==, 64);

/*
 * Initialize a capability map.
 * Returns true on success, false on allocation failure.
 */
bool capability_map_init(CapabilityMap *map);

/*
 * Insert a new key-value pair into the map, or update the value if the key exists.
 * Returns true on success, false on allocation failure (during resize).
 */
bool capability_map_insert(CapabilityMap *map, uint64_t key, void *value);

/*
 * Look up a key in the map.
 * Returns the stored value, or NULL if the key is not found.
 */
void *capability_map_lookup(CapabilityMap *map, uint64_t key);

/*
 * Delete a key from the map.
 * Returns true if the key was present and deleted, false otherwise.
 */
bool capability_map_delete(CapabilityMap *map, uint64_t key);

/*
 * Rebuild table to remove tombstones.
 * Returns true on success, false on allocation failure.
 */
bool capability_map_cleanup(CapabilityMap *map);

#endif // __ANOS_KERNEL_PROCESS_CAPABILITY_MAP_H
