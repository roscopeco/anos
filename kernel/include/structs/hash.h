/*
 * stage3 - general hash table
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This is a basic hash table using open‐addressing with linear probing.
 * 
 * The FBA is used for allocation. Resizing could be more efficient, but
 * lookup is amortized constant-time and resize should be relatively
 * rare...
 * 
 * TODO this could be significantly improved - it's written for simplicity
 * right now, but really ought to be tuned...
 * 
 * Performance of this isn't _great_ and all operations use spinlocks,
 * so it shouldn't be used for high-contention situations.
 */

#ifndef __ANOS_KERNEL_HASH_H
#define __ANOS_KERNEL_HASH_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "anos_assert.h"
#include "spinlock.h"
#include "vmm/vmconfig.h"

#define ENTRIES_PER_PAGE ((VM_PAGE_SIZE / sizeof(HashEntry)))

typedef struct {
    uint64_t key;
    void *data;
} HashEntry;

typedef struct {
    size_t capacity;
    size_t size;
    SpinLock *lock;
    HashEntry *entries;
} HashTable;

static_assert_sizeof(HashEntry, <=, 64);
static_assert_sizeof(HashTable, <=, 64);

/*
 * Create a new hash table with backing storage for num_pages pages.
 */
HashTable *hash_table_create(size_t num_pages);

/*
 * Insert a new (key, value) pair into the table.
 *
 * Resizes dynamically if the load factor would be exceeded.
 * Returns true on success or false if a duplicate is found or on failure.
 */
bool hash_table_insert(HashTable *ht, uint64_t key, void *value);

/*
 * Lookup a value pointer by its key.
 * 
 * Returns the value pointer if found, or NULL if not present.
 */
void *hash_table_lookup(HashTable *ht, uint64_t key);

/*
 * Remove an entry identified by key from the table.
 *
 * Returns the removed value if the key was found and removed, 
 * or NULL if not found.
 *
 * After removal, rehashes the contiguous cluster to preserve lookups.
 */
void *hash_table_remove(HashTable *ht, uint64_t key);

#endif //__ANOS_KERNEL_HASH_H
