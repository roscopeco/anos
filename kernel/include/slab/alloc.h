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
#include <stdbool.h>
#include <stdint.h>

typedef struct Slab {
    ListNode this;
    uint64_t reserved;
    uint64_t bitmap0;
    uint64_t bitmap1;
    uint64_t bitmap2;
    uint64_t bitmap3;
} Slab;

static inline Slab *slab_base(void *block_addr) {
    // TODO check block_addr is in the FBA area!
    return (Slab *)(((uintptr_t)block_addr) & ~(0x3fff));
}

bool slab_alloc_init();

void *slab_alloc_block();
void *slab_alloc_blocks();

void slab_free_block(void *block);
void slab_free_blocks(void *first_block);

#endif //__ANOS_KERNEL_SLAB_ALLOC_H