/*
 * stage3 - The slab allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * The slab allocator sits on top of the fixed block
 * allocator and allocates 16KiB slabs carved up into
 * blocks of 64 bytes (256 blocks per slab).
 *
 * The top block in each slab is reserved for metadata,
 * and slabs form a linked list within the kernel's slab
 * space.
 */

#ifndef __ANOS_KERNEL_SLAB_ALLOC_H
#define __ANOS_KERNEL_SLAB_ALLOC_H

#include "structs/list.h"
#include "vmm/vmconfig.h"
#include <stdbool.h>
#include <stdint.h>

static const uint64_t BYTES_PER_SLAB = 16384; // 16KiB Slabs
static const uint64_t SLAB_BASE_MASK = ~(BYTES_PER_SLAB - 1);
static const uint8_t SLAB_BLOCK_SIZE = 64; // 64-byte blocks
static const uint64_t BLOCKS_PER_SLAB = BYTES_PER_SLAB / SLAB_BLOCK_SIZE;

typedef struct Slab {
    ListNode this;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t bitmap0;
    uint64_t bitmap1;
    uint64_t bitmap2;
    uint64_t bitmap3;
} Slab;

static inline Slab *slab_base(void *block_addr) {
    // TODO check block_addr is in the FBA area!
    return (Slab *)(((uintptr_t)block_addr) & SLAB_BASE_MASK);
}

bool slab_alloc_init();

void *slab_alloc_block();

void slab_free(void *block);

#endif //__ANOS_KERNEL_SLAB_ALLOC_H