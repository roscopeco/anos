/*
 * stage3 - General bitmap
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_STRUCTS_BITMAP_H
#define __ANOS_KERNEL_STRUCTS_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

static inline void bitmap_set(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] |= (1ULL << (bit & 0x3f));
}

static inline void bitmap_clear(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] &= ~(1ULL << (bit & 0x3f));
}

static inline void bitmap_flip(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] ^= (1ULL << (bit & 0x3f));
}

static inline bool bitmap_check(uint64_t *bitmap, uint64_t bit) {
    return bitmap[bit >> 6] & (1ULL << (bit & 0x3f));
}

#endif //__ANOS_KERNEL_STRUCTS_BITMAP_H
