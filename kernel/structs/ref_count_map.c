/*
 * stage3 - Hashtable for shared phys mem tracking
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "fba/alloc.h"
#include "machine.h"
#include "slab/alloc.h"
#include "spinlock.h"

#include "structs/ref_count_map.h"

#define INITIAL_SIZE 1024

#ifndef NULL
#define NULL (((void *)0))
#endif

#define SPINLOCK_LOCK()                                                        \
    uint64_t intflags = spinlock_lock_irqsave(&refcount_map_lock)

#define SPINLOCK_UNLOCK()                                                      \
    spinlock_unlock_irqrestore(&refcount_map_lock, intflags)

// Global map instance and lock for now...
static RefCountMap *global_refcount_map = NULL;
static SpinLock refcount_map_lock;

static inline uint64_t hash_address(uintptr_t addr, uint64_t size) {
    const uint64_t golden_ratio = 0x9E3779B97F4A7C15ULL;
    return ((addr * golden_ratio) >> 32) % size;
}

#define PTRS_PER_BLOCK (4096 / sizeof(Entry *))

static BlockNode *add_block(RefCountMap *map) {
    BlockNode *node = slab_alloc_block();
    if (!node)
        return NULL;

    void *block = fba_alloc_block();
    if (!block) {
        slab_free(node);
        return NULL;
    }

    node->block = block;
    node->next = map->block_list;
    node->used = 0;
    map->block_list = node;

    return node;
}

static Entry **allocate_bucket_array(uint64_t size, RefCountMap *map) {
    uint64_t total_size = size * sizeof(Entry *);
    uint64_t blocks_needed = (total_size + 4095) / 4096;

    BlockNode *first_node = add_block(map);
    if (!first_node)
        return NULL;

    Entry **buckets = (Entry **)first_node->block;
    uint64_t ptrs_remaining = size;
    uint64_t current_block_ptrs = PTRS_PER_BLOCK;
    Entry **current_ptr = buckets;

    for (uint64_t i = 0; i < current_block_ptrs && i < size; i++) {
        current_ptr[i] = NULL;
    }
    first_node->used = (current_block_ptrs * sizeof(Entry *));
    ptrs_remaining -= current_block_ptrs;

    BlockNode *current_node = first_node;
    while (ptrs_remaining > 0) {
        BlockNode *new_node = add_block(map);
        if (!new_node) {
            // Cleanup on failure
            while (map->block_list) {
                BlockNode *to_free = map->block_list;
                map->block_list = to_free->next;
                fba_free(to_free->block);
                slab_free(to_free);
            }
            return NULL;
        }

        current_block_ptrs = (ptrs_remaining < PTRS_PER_BLOCK) ? ptrs_remaining
                                                               : PTRS_PER_BLOCK;
        Entry **block_ptr = (Entry **)new_node->block;
        for (uint64_t i = 0; i < current_block_ptrs; i++) {
            block_ptr[i] = NULL;
        }

        new_node->used = (current_block_ptrs * sizeof(Entry *));
        ptrs_remaining -= current_block_ptrs;
    }

    return buckets;
}

static Entry **get_bucket_ptr(RefCountMap *map, uint64_t idx) {
    uint64_t block_idx = idx / PTRS_PER_BLOCK;
    uint64_t offset = idx % PTRS_PER_BLOCK;

    BlockNode *current = map->block_list;
    for (uint64_t i = 0; i < block_idx && current; i++) {
        current = current->next;
    }

    if (!current)
        return NULL;

    Entry **block_ptrs = (Entry **)current->block;
    return &block_ptrs[offset];
}

bool refcount_map_init(void) {
    SPINLOCK_LOCK();

    if (global_refcount_map != NULL) {
        SPINLOCK_UNLOCK();
        return true; // Already initialized
    }

    RefCountMap *map = slab_alloc_block();
    if (!map) {
        SPINLOCK_UNLOCK();
        return false;
    }

    map->size = INITIAL_SIZE;
    map->num_entries = 0;
    map->block_list = NULL;

    map->buckets = allocate_bucket_array(INITIAL_SIZE, map);
    if (!map->buckets) {
        slab_free(map);
        SPINLOCK_UNLOCK();
        return false;
    }

    global_refcount_map = map;
    SPINLOCK_UNLOCK();
    return true;
}

static bool resize_map(RefCountMap *map) {
    uint64_t new_size = map->size * 2;

    RefCountMap *new_map = slab_alloc_block();
    if (!new_map)
        return false;

    new_map->size = new_size;
    new_map->num_entries = map->num_entries;
    new_map->block_list = NULL;

    new_map->buckets = allocate_bucket_array(new_size, new_map);
    if (!new_map->buckets) {
        slab_free(new_map);
        return false;
    }

    // Rehash all existing entries
    for (uint64_t i = 0; i < map->size; i++) {
        Entry **bucket_ptr = get_bucket_ptr(map, i);
        Entry *entry = *bucket_ptr;

        while (entry) {
            Entry *next = entry->next;
            uint64_t new_idx = hash_address(entry->physical_addr, new_size);
            Entry **new_bucket = get_bucket_ptr(new_map, new_idx);

            entry->next = *new_bucket;
            *new_bucket = entry;

            entry = next;
        }
    }

    // Free old blocks
    while (map->block_list) {
        BlockNode *to_free = map->block_list;
        map->block_list = to_free->next;
        fba_free(to_free->block);
        slab_free(to_free);
    }

    // Transfer new map's data to old map
    map->buckets = new_map->buckets;
    map->size = new_map->size;
    map->block_list = new_map->block_list;

    slab_free(new_map);
    return true;
}

uint32_t refcount_map_increment(uintptr_t addr) {
    SPINLOCK_LOCK();

    if (!global_refcount_map) {
        SPINLOCK_UNLOCK();
        return 0;
    }

    RefCountMap *map = global_refcount_map;
    uint64_t idx = hash_address(addr, map->size);
    Entry **bucket_ptr = get_bucket_ptr(map, idx);
    Entry *entry = *bucket_ptr;

    // Check if entry exists
    while (entry) {
        if (entry->physical_addr == addr && entry->is_occupied) {
            entry->ref_count++;
            uint32_t result = entry->ref_count;
            SPINLOCK_UNLOCK();
            return result;
        }
        entry = entry->next;
    }

    // Check if resize needed
    if (4 * map->num_entries >= 3 * map->size) { // == 0.75 load factor...
        if (!resize_map(map)) {
            SPINLOCK_UNLOCK();
            return 0;
        }
        idx = hash_address(addr, map->size);
        bucket_ptr = get_bucket_ptr(map, idx);
    }

    // Create new entry
    Entry *new_entry = slab_alloc_block();
    if (!new_entry) {
        SPINLOCK_UNLOCK();
        return 0;
    }

    new_entry->physical_addr = addr;
    new_entry->ref_count = 1;
    new_entry->is_occupied = true;
    new_entry->next = *bucket_ptr;
    *bucket_ptr = new_entry;
    map->num_entries++;

    SPINLOCK_UNLOCK();
    return 1;
}

uint32_t refcount_map_decrement(uintptr_t addr) {
    SPINLOCK_LOCK();

    if (!global_refcount_map) {
        SPINLOCK_UNLOCK();
        return 0;
    }

    RefCountMap *map = global_refcount_map;
    uint64_t idx = hash_address(addr, map->size);
    Entry **bucket_ptr = get_bucket_ptr(map, idx);
    Entry *entry = *bucket_ptr;
    Entry *prev = NULL;

    while (entry) {
        if (entry->physical_addr == addr && entry->is_occupied) {
            entry->ref_count--;

            if (entry->ref_count == 0) {
                if (prev) {
                    prev->next = entry->next;
                } else {
                    *bucket_ptr = entry->next;
                }
                slab_free(entry);
                map->num_entries--;
                SPINLOCK_UNLOCK();
                return 0;
            }

            uint32_t result = entry->ref_count;
            SPINLOCK_UNLOCK();
            return result;
        }
        prev = entry;
        entry = entry->next;
    }

    SPINLOCK_UNLOCK();
    return 0; // Address not found
}

// TODO probably don't need this..?
void refcount_map_cleanup(void) {
    SPINLOCK_LOCK();

    if (!global_refcount_map) {
        SPINLOCK_UNLOCK();
        return;
    }

    RefCountMap *map = global_refcount_map;

    // Free all entries
    for (uint64_t i = 0; i < map->size; i++) {
        Entry **bucket_ptr = get_bucket_ptr(map, i);
        Entry *entry = *bucket_ptr;
        while (entry) {
            Entry *next = entry->next;
            slab_free(entry);
            entry = next;
        }
    }

    // Free all blocks
    while (map->block_list) {
        BlockNode *to_free = map->block_list;
        map->block_list = to_free->next;
        fba_free(to_free->block);
        slab_free(to_free);
    }

    slab_free(map);
    global_refcount_map = NULL;

    SPINLOCK_UNLOCK();
}