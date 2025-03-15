/*
 * stage3 - Named channels
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "ipc/channel.h"
#include "spinlock.h"
#include "structs/hash.h"
#include "structs/strhash.h"

#define INITIAL_PAGE_COUNT ((2))

static HashTable *name_table;

// In case you're wondering why no lock, HashTable has built-in locking,
// and that's good enough for this use case...

// Using SDBM hashing for better bit mixing. SDBM’s arithmetic (using
// shifts by 6 and 16 bits, then subtracting the hash) tends to produce
// a better avalanche effect.
//
// This improved bit diffusion _should_ lead to a more uniform distribution
// of hash values, but this'll bear some testing to be sure...
//
// (This is especially important since currently there's no collision
// support in the hash table, i.e. it doesn't bucket values by hash).

void named_channel_init(void) {
    name_table = hash_table_create(INITIAL_PAGE_COUNT);
}

bool named_channel_register(uint64_t cookie, char *name) {
    if (!ipc_channel_exists(cookie)) {
        return false;
    }

    uint64_t name_hash = str_hash_sdbm((unsigned char *)name, 255);
    return hash_table_insert(name_table, name_hash, (void *)cookie);
}

uint64_t named_channel_find(char *name) {
    uint64_t name_hash = str_hash_sdbm((unsigned char *)name, 255);
    return (uint64_t)hash_table_lookup(name_table, name_hash);
}

uint64_t named_channel_deregister(char *name) {
    uint64_t name_hash = str_hash_sdbm((unsigned char *)name, 255);
    return (uint64_t)hash_table_remove(name_table, name_hash);
}
