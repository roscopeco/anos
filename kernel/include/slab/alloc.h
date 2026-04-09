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

// clang-format Language: C

#ifndef __ANOS_KERNEL_SLAB_ALLOC_H
#define __ANOS_KERNEL_SLAB_ALLOC_H

#include <stdbool.h>
#include <stdint.h>

#include "anos_assert.h"
#include "structs/list.h"
#include "vmm/vmconfig.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang still doesn't support constexpr - Apr 2026
#define BYTES_PER_SLAB ((16384)) // 16KiB Slabs
#define SLAB_BASE_MASK ((~(BYTES_PER_SLAB - 1)))
#define SLAB_BLOCK_SIZE ((64)) // 64-byte blocks
#define BLOCKS_PER_SLAB ((BYTES_PER_SLAB / SLAB_BLOCK_SIZE))
#else
static constexpr uint64_t BYTES_PER_SLAB = 16384; // 16KiB Slabs
static constexpr uint64_t SLAB_BASE_MASK = ~(BYTES_PER_SLAB - 1);
static constexpr uint8_t SLAB_BLOCK_SIZE = 64; // 64-byte blocks
static constexpr uint64_t BLOCKS_PER_SLAB = BYTES_PER_SLAB / SLAB_BLOCK_SIZE;
#endif

typedef struct Slab {
    ListNode this;
    uint64_t reserved0;
    uint64_t reserved1;
    uint64_t reserved2;
    uint64_t bitmap0;
    uint64_t bitmap1;
    uint64_t bitmap2;
    uint64_t bitmap3;
} Slab;

static_assert_sizeof(Slab, ==, SLAB_BLOCK_SIZE);

static inline Slab *slab_base(void *block_ptr) {
    const uintptr_t block_addr = (uintptr_t)block_ptr;

    // TODO We need to range check this, but tests are making that a PITA...
    //      Fix the tests then range change correctly here!!
    //if (block_addr >= KERNEL_FBA_BEGIN && block_addr < KERNEL_FBA_END) {
    return (Slab *)(block_addr & SLAB_BASE_MASK);
    //}
}

bool slab_alloc_init();

void *slab_alloc_block();

void slab_free(void *block);

#endif //__ANOS_KERNEL_SLAB_ALLOC_H