
/*
 * stage3 - General hashing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * See here for sources: http://www.cse.yorku.ca/~oz/hash.html
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_STRHASH_H
#define __ANOS_KERNEL_STRHASH_H

#include <stddef.h>
#include <stdint.h>

#define str_hash str_hash_djb2

/*
 * djb2; this algorithm (k=33) was first reported by Dan Bernstein
 * in comp.lang.c many years ago.
 * 
 * The magic of number 33 (why it works better than many other constants,
 * prime or not) has never been adequately explained.
 */
static inline uint64_t str_hash_djb2(unsigned char *str, size_t max_len) {
    unsigned long hash = 5381;
    size_t pos = 0;
    int c;

    if (!str) {
        return hash;
    }

    while ((c = *str++) && pos++ < max_len) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/*
 * This algorithm was created for sdbm (a public-domain reimplementation of ndbm)
 * database library. It was found to do well in scrambling bits, causing better
 * distribution of the keys and fewer splits. 
 * 
 * It also happens to be a good general hashing function with good distribution.
 * The actual function is:
 * 
 * ```
 * hash(i) = hash(i - 1) * 65599 + str[i];
 * ```
 * 
 * What is included below is the faster version used in gawk. The magic prime 
 * constant 65599 (2^6 + 2^16 - 1) was picked out of thin air while experimenting
 * with many different constants. 
 * 
 * This is one of the algorithms used in berkeley db (see sleepycat) and elsewhere.
 */
static inline uint64_t str_hash_sdbm(unsigned char *str, size_t max_len) {
    unsigned long hash = 0;
    size_t pos = 0;
    int c;

    if (!str) {
        return hash;
    }

    while ((c = *str++) && pos++ < max_len) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

#endif //__ANOS_KERNEL_STRHASH_H
