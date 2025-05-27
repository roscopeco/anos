/*
 * stage3 - Capability-specific hashmap
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "std/string.h"

#include "fba/alloc.h"
#include "slab/alloc.h"
#include "spinlock.h"

#include "capabilities/map.h"

#define INITIAL_CAPACITY 64
#define MAX_LOAD_FACTOR 0.75

static uint64_t hash_u64(uint64_t x) {
    // MurmurHash3 finalizer
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53;
    x ^= x >> 33;
    return x;
}

static bool resize(CapabilityMap *map, size_t new_capacity) {
    size_t new_bytes = new_capacity * sizeof(CapabilityMapEntry);
    size_t blocks = (new_bytes + 4095) / 4096;

    CapabilityMapEntry *new_entries = fba_alloc_blocks(blocks);
    if (!new_entries)
        return false;

    memset(new_entries, 0, blocks * 4096);

    for (size_t i = 0; i < map->capacity; ++i) {
        CapabilityMapEntry *old_entry = &map->entries[i];
        if (old_entry->occupied && !old_entry->tombstone) {
            uint64_t h = hash_u64(old_entry->key);
            size_t j = h & (new_capacity - 1);
            while (new_entries[j].occupied) {
                j = (j + 1) & (new_capacity - 1);
            }
            new_entries[j] = *old_entry;
        }
    }

    fba_free_blocks(map->entries, map->block_count);

    map->entries = new_entries;
    map->capacity = new_capacity;
    map->block_count = blocks;

    return true;
}

bool capability_map_init(CapabilityMap *map) {
    if (!map) {
        return false;
    }

    memset(map, 0, sizeof(*map));

    map->lock = slab_alloc_block();

    if (!map->lock) {
        return false;
    }

    spinlock_init(map->lock);

    size_t bytes = INITIAL_CAPACITY * sizeof(CapabilityMapEntry);
    size_t blocks = (bytes + 4095) / 4096;

    map->entries = fba_alloc_blocks(blocks);

    if (!map->entries) {
        return false;
    }

    memset(map->entries, 0, blocks * 4096);

    map->capacity = INITIAL_CAPACITY;
    map->block_count = blocks;
    map->size = 0;

    return true;
}

bool capability_map_insert(CapabilityMap *map, uint64_t key, void *value) {
    uint64_t flags = spinlock_lock_irqsave(map->lock);

    if ((double)(map->size + 1) / map->capacity > MAX_LOAD_FACTOR) {
        if (!resize(map, map->capacity * 2)) {
            spinlock_unlock_irqrestore(map->lock, flags);
            return false;
        }
    }

    uint64_t h = hash_u64(key);
    size_t i = h & (map->capacity - 1);

    size_t first_tombstone = SIZE_MAX;

    while (map->entries[i].occupied) {
        if (!map->entries[i].tombstone && map->entries[i].key == key) {
            map->entries[i].value = value;
            spinlock_unlock_irqrestore(map->lock, flags);
            return true;
        }

        if (map->entries[i].tombstone && first_tombstone == SIZE_MAX) {
            first_tombstone = i;
        }

        i = (i + 1) & (map->capacity - 1);
    }

    size_t insert_at = (first_tombstone != SIZE_MAX) ? first_tombstone : i;
    map->entries[insert_at].key = key;
    map->entries[insert_at].value = value;
    map->entries[insert_at].occupied = true;
    map->entries[insert_at].tombstone = false;
    map->size++;

    spinlock_unlock_irqrestore(map->lock, flags);
    return true;
}

void *capability_map_lookup(CapabilityMap *map, uint64_t key) {
    if (!map->entries) {
        return NULL;
    }

    uint64_t flags = spinlock_lock_irqsave(map->lock);

    uint64_t h = hash_u64(key);
    size_t i = h & (map->capacity - 1);

    while (map->entries[i].occupied) {
        if (!map->entries[i].tombstone && map->entries[i].key == key) {
            void *val = map->entries[i].value;
            spinlock_unlock_irqrestore(map->lock, flags);
            return val;
        }
        i = (i + 1) & (map->capacity - 1);
    }

    spinlock_unlock_irqrestore(map->lock, flags);
    return NULL;
}

bool capability_map_delete(CapabilityMap *map, uint64_t key) {
    if (!map->entries) {
        return false;
    }

    uint64_t flags = spinlock_lock_irqsave(map->lock);

    uint64_t h = hash_u64(key);
    size_t i = h & (map->capacity - 1);

    while (map->entries[i].occupied) {
        if (!map->entries[i].tombstone && map->entries[i].key == key) {
            map->entries[i].tombstone = true;
            map->entries[i].value = NULL;
            map->size--;
            spinlock_unlock_irqrestore(map->lock, flags);
            return true;
        }
        i = (i + 1) & (map->capacity - 1);
    }

    spinlock_unlock_irqrestore(map->lock, flags);
    return false;
}

// Cleanup: rebuild table to remove tombstones
bool capability_map_cleanup(CapabilityMap *map) {
    uint64_t flags = spinlock_lock_irqsave(map->lock);
    bool result = resize(map, map->capacity);
    spinlock_unlock_irqrestore(map->lock, flags);

    return result;
}
