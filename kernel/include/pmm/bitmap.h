/*
 * stage3 - General bitmap
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PMM_BITMAP_H
#define __ANOS_KERNEL_PMM_BITMAP_H

#include <stdint.h>

void bitmap_set(uint64_t *bitmap, uint64_t bit);
void bitmap_clear(uint64_t *bitmap, uint64_t bit);
void bitmap_flip(uint64_t *bitmap, uint64_t bit);

#endif//__ANOS_KERNEL_PMM_BITMAP_H
