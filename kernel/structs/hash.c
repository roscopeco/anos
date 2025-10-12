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

#define SOFT_BARRIER() __asm__ __volatile__("" ::: "memory");

#ifdef UNIT_TESTS
#define LOCK(ht)                                                                                                       \
    do {                                                                                                               \
        spinlock_lock(ht->lock);                                                                                       \
        SOFT_BARRIER()                                                                                                 \
    } while (0)
#define UNLOCK(ht)                                                                                                     \
    do {                                                                                                               \
        SOFT_BARRIER()                                                                                                 \
        spinlock_unlock(ht->lock);                                                                                     \
    } while (0)
#else
#define LOCK(ht)                                                                                                       \
    uint64_t __irqsave;                                                                                                \
    do {                                                                                                               \
        __irqsave = spinlock_lock_irqsave(ht->lock);                                                                   \
        SOFT_BARRIER()                                                                                                 \
    } while (0)

#define UNLOCK(ht)                                                                                                     \
    do {                                                                                                               \
        SOFT_BARRIER()                                                                                                 \
        spinlock_unlock_irqrestore(ht->lock, __irqsave);                                                               \
    } while (0)
#endif

// Internal helper: Resize the hash table by doubling the number of pages.
// This function must be called with the hash table lock held.
static bool hash_table_resize(HashTable *ht) {
    // Calculate new parameters.
    const size_t current_pages = ht->capacity / ENTRIES_PER_PAGE;
    const size_t new_pages = current_pages * 2;
    const size_t new_capacity = new_pages * ENTRIES_PER_PAGE;

    // Allocate new backing storage.
    HashEntry *new_entries = (HashEntry *)fba_alloc_blocks((int)new_pages);
    if (!new_entries)
        return false;

    // Initialize new entries to empty.
    for (size_t i = 0; i < new_capacity; i++) {
        new_entries[i].key = 0;
        new_entries[i].data = NULL;
    }

    HashEntry *old_entries = ht->entries;
    __asm__ __volatile__("" ::: "memory");

    // Rehash all current entries into the new array.
    for (size_t i = 0; i < ht->capacity; i++) {
        const uint64_t key = old_entries[i].key;
        void *value = old_entries[i].data;

        if (key != 0) {
            const size_t index = key % new_capacity;

            for (size_t j = 0; j < new_capacity; j++) {
                const size_t pos = (index + j) % new_capacity;

                if (new_entries[pos].key == 0) {
                    new_entries[pos].key = key;
                    new_entries[pos].data = value;
                    break;
                }
            }
        }
    }

    // Free the old backing storage and update the table.
    fba_free(old_entries);

    __asm__ __volatile__("" ::: "memory");
    ht->entries = new_entries;
    ht->capacity = new_capacity;
    __asm__ __volatile__("" ::: "memory");

    return true;
}

HashTable *hash_table_create(const size_t num_pages) {
    // Allocate the hash table structure from the slab (must be <= 64 bytes).
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
        slab_free(ht);
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

bool hash_table_insert(HashTable *ht, const uint64_t key, void *value) {
    bool ret = false;
    LOCK(ht);

    // Resize if adding one more entry would exceed our load factor.
    // Do this BEFORE checking for duplicates to avoid use-after-free.
    if ((ht->size + 1) > (size_t)(ht->capacity * LOAD_FACTOR)) {
        if (!hash_table_resize(ht)) {
            UNLOCK(ht);
            return false;
        }
    }

    HashEntry *entries = ht->entries;
    const size_t capacity = ht->capacity;
    __asm__ __volatile__("" ::: "memory");

    // Check if the key already exists and find an insertion point.
    const size_t index = key % capacity;
    for (size_t i = 0; i < capacity; i++) {
        const size_t pos = (index + i) % capacity;

        if (entries[pos].key == key) {
            // Key already exists
            UNLOCK(ht);
            return false;
        }

        if (entries[pos].key == 0) {
            // Found empty slot, insert here
            entries[pos].key = key;
            entries[pos].data = value;
            ht->size++;
            ret = true;
            break;
        }
    }

    UNLOCK(ht);
    return ret;
}

void *hash_table_lookup(const HashTable *ht, const uint64_t key) {
    void *result = NULL;
    LOCK(ht);

    HashEntry *entries = ht->entries;
    const size_t capacity = ht->capacity;
    __asm__ __volatile__("" ::: "memory");

    const size_t index = key % capacity;

    for (size_t i = 0; i < capacity; i++) {
        const size_t pos = (index + i) % capacity;

        if (entries[pos].key == 0) {
            break; // empty slot means not found
        }

        if (entries[pos].key == key) {
            result = entries[pos].data;
            break;
        }
    }

    UNLOCK(ht);
    return result;
}

void *hash_table_remove(HashTable *ht, const uint64_t key) {
    void *ret = NULL;
    LOCK(ht);

    // Load entries pointer and capacity with (hopefully) proper ordering
    HashEntry *entries = ht->entries;
    const size_t capacity = ht->capacity;
    __asm__ __volatile__("" ::: "memory");

    // fuck me this is a mess...

    const size_t index = key % capacity;

    for (size_t i = 0; i < capacity; i++) {
        const size_t pos = (index + i) % capacity;
        if (entries[pos].key == 0) {
            break;
        }

        if (entries[pos].key == key) {
            // save the entry for return...
            ret = entries[pos].data;

            // and remove it from the table
            entries[pos].key = 0;
            entries[pos].data = NULL;
            ht->size--;

            // Rehash the following cluster.
            size_t j = (pos + 1) % capacity;
            while (entries[j].key != 0) {
                const uint64_t rehash_key = entries[j].key;
                void *rehash_value = entries[j].data;

                entries[j].key = 0;
                entries[j].data = NULL;
                ht->size--;

                const size_t k = rehash_key % capacity;

                for (size_t n = 0; n < capacity; n++) {
                    const size_t new_pos = (k + n) % capacity;
                    if (entries[new_pos].key == 0) {
                        entries[new_pos].key = rehash_key;
                        entries[new_pos].data = rehash_value;
                        ht->size++;
                        break;
                    }
                }

                j = (j + 1) % capacity;
            }

            break;
        }
    }

    UNLOCK(ht);
    return ret;
}
