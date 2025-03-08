/*
 * stage3 - general hash table
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "structs/hash.h"

#include "fba/alloc.h"
#include "slab/alloc.h"
#include "spinlock.h"
#include "std/string.h"
#include "vmm/vmconfig.h"

#define TOMBSTONE ((uint64_t)-1)
#define LOAD_FACTOR 0.75

#ifdef UNIT_TESTS
#define LOCK(ht) spinlock_lock(ht->lock)
#define UNLOCK(ht) spinlock_unlock(ht->lock)
#else
#define LOCK(ht) uint64_t __irqsave = spinlock_lock_irqsave(ht->lock)
#define UNLOCK(ht) spinlock_unlock_irqrestore(ht->lock, __irqsave)
#endif

// Internal helper: Resize the hash table by doubling the number of pages.
// This function must be called with the hash table lock held.
static bool hash_table_resize(HashTable *ht) {
    // Calculate new parameters.
    size_t current_pages = ht->capacity / ENTRIES_PER_PAGE;
    size_t new_pages = current_pages * 2;
    size_t new_capacity = new_pages * ENTRIES_PER_PAGE;

    // Allocate new backing storage.
    HashEntry *new_entries = (HashEntry *)fba_alloc_blocks((int)new_pages);
    if (!new_entries)
        return false;

    // Initialize new entries to empty.
    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = 0;
        new_entries[i].data = NULL;
    }

    // Rehash all current entries into the new array.
    for (size_t i = 0; i < ht->capacity; i++) {
        uint64_t key = ht->entries[i].key;
        void *value = ht->entries[i].data;
        if (key != 0) {
            size_t index = key % new_capacity;
            for (size_t j = 0; j < new_capacity; j++) {
                size_t pos = (index + j) % new_capacity;
                if (new_entries[pos].key == 0) {
                    new_entries[pos].key = key;
                    new_entries[pos].data = value;
                    break;
                }
            }
        }
    }

    // Free the old backing storage and update the table.
    fba_free(ht->entries);
    ht->entries = new_entries;
    ht->capacity = new_capacity;

    return true;
}

HashTable *hash_table_create(size_t num_pages) {
    // Allocate the hash table structure from the slab (must be ≤ 64 bytes).
    HashTable *ht = (HashTable *)slab_alloc_block();
    if (!ht)
        return NULL;

    // Allocate the backing storage for entries.
    ht->entries = (HashEntry *)fba_alloc_blocks((int)num_pages);
    if (!ht->entries) {
        fba_free(ht);
        return NULL;
    }
    ht->capacity = num_pages * ENTRIES_PER_PAGE;
    ht->size = 0;

    // Allocate and initialize the spinlock.
    ht->lock = (SpinLock *)slab_alloc_block();
    if (!ht->lock) {
        fba_free(ht->entries);
        fba_free(ht);
        return NULL;
    }
    spinlock_init(ht->lock);

    // Initialize each entry to empty.
    for (size_t i = 0; i < ht->capacity; i++) {
        ht->entries[i].key = 0;
        ht->entries[i].data = NULL;
    }

    return ht;
}

bool hash_table_insert(HashTable *ht, uint64_t key, void *value) {
    bool ret = false;
    LOCK(ht);

    // Check if the key already exists.
    size_t index = key % ht->capacity;
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t pos = (index + i) % ht->capacity;
        if (ht->entries[pos].key == key) {
            UNLOCK(ht);
            return false;
        }
    }

    // Resize if adding one more entry would exceed our load factor.
    if ((ht->size + 1) > (size_t)(ht->capacity * LOAD_FACTOR)) {
        if (!hash_table_resize(ht)) {
            UNLOCK(ht);
            return false;
        }
    }

    // Recalculate index in case capacity changed.
    index = key % ht->capacity;
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t pos = (index + i) % ht->capacity;
        if (ht->entries[pos].key == 0) {
            ht->entries[pos].key = key;
            ht->entries[pos].data = value;
            ht->size++;
            ret = true;
            break;
        }
    }

    UNLOCK(ht);
    return ret;
}

void *hash_table_lookup(HashTable *ht, uint64_t key) {
    void *result = NULL;
    LOCK(ht);

    size_t index = key % ht->capacity;
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t pos = (index + i) % ht->capacity;
        if (ht->entries[pos].key == 0)
            break; // empty slot means not found
        if (ht->entries[pos].key == key) {
            result = ht->entries[pos].data;
            break;
        }
    }

    UNLOCK(ht);
    return result;
}

bool hash_table_remove(HashTable *ht, uint64_t key) {
    bool ret = false;
    LOCK(ht);

    size_t index = key % ht->capacity;
    size_t pos;
    for (size_t i = 0; i < ht->capacity; i++) {
        pos = (index + i) % ht->capacity;
        if (ht->entries[pos].key == 0)
            goto out;
        if (ht->entries[pos].key == key) {
            // Remove the entry.
            ht->entries[pos].key = 0;
            ht->entries[pos].data = NULL;
            ht->size--;
            ret = true;

            // Rehash the following cluster.
            size_t j = (pos + 1) % ht->capacity;
            while (ht->entries[j].key != 0) {
                uint64_t rehash_key = ht->entries[j].key;
                void *rehash_value = ht->entries[j].data;
                ht->entries[j].key = 0;
                ht->entries[j].data = NULL;
                ht->size--;

                size_t k = rehash_key % ht->capacity;
                for (size_t n = 0; n < ht->capacity; n++) {
                    size_t new_pos = (k + n) % ht->capacity;
                    if (ht->entries[new_pos].key == 0) {
                        ht->entries[new_pos].key = rehash_key;
                        ht->entries[new_pos].data = rehash_value;
                        ht->size++;
                        break;
                    }
                }
                j = (j + 1) % ht->capacity;
            }
            break;
        }
    }
out:
    UNLOCK(ht);
    return ret;
}
