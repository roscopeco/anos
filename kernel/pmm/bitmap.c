/*
 * stage3 - General bitmap
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

void bitmap_set(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] |= (1ULL << (bit & 0x3f));
}

void bitmap_clear(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] &= ~(1ULL << (bit & 0x3f));
}

void bitmap_flip(uint64_t *bitmap, uint64_t bit) {
    bitmap[bit >> 6] ^= (1ULL << (bit & 0x3f));
}
