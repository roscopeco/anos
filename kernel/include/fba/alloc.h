/*
 * stage3 - The fixed-block allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * The fixed-block allocator manages allocation and
 * deallocation of physical memory and virtual address
 * space to map page-size (4KiB) blocks into the
 * kernel's 1GiB FBA space at 0xffffffffc0000000.
 */

#ifndef __ANOS_KERNEL_PMM_FBA_ALLOC_H
#define __ANOS_KERNEL_PMM_FBA_ALLOC_H

#include <stdbool.h>
#include <stdint.h>

// FBA_BEGIN **must** be aligned on 256KiB (64 page) boundary at least!
#define KERNEL_FBA_BEGIN ((0xffffffffc0000000))
#define KERNEL_FBA_SIZE ((0x40000000))
#define KERNEL_FBA_BLOCK_SIZE VM_PAGE_SIZE
#define KERNEL_FBA_SIZE_BLOCKS ((KERNEL_FBA_SIZE / KERNEL_FBA_BLOCK_SIZE))
#define KERNEL_FBA_END ((KERNEL_FBA_BEGIN + KERNEL_FBA_SIZE - 1))

bool fba_init(uint64_t *pml4, uintptr_t fba_begin, uint64_t fba_size_blocks);

void *fba_alloc_blocks_aligned(uint32_t count, uint8_t page_align);
void *fba_alloc_blocks(uint32_t count);
void *fba_alloc_block();
void fba_free(void *block);

#endif //__ANOS_KERNEL_PMM_FBA_ALLOC_H