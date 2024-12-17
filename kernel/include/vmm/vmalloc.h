/*
 * stage3 - The virtual address space allocator
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PMM_VMALLOC_H
#define __ANOS_KERNEL_PMM_VMALLOC_H

#include <stdbool.h>
#include <stdint.h>

// Size of a page in bytes
#define PAGE_SIZE 4096

// Error codes
#define VMM_SUCCESS 0
#define VMM_ERROR_NO_SPACE -1
#define VMM_ERROR_INVALID_PARAMS -2
#define VMM_ERROR_NOT_INITIALIZED -3

// Initialize the VMM allocator
// metadata_start: Start of the region where we can store our tracking data
// metadata_size: Size of the region available for tracking data
// managed_start: Start of the virtual address range to manage
// managed_size: Size of the virtual address range to manage
int vmm_init(void *metadata_start, uint64_t metadata_size,
             uint64_t managed_start, uint64_t managed_size);

// Allocate a block of contiguous pages
// Returns the base address of the allocated block, or 0 on failure
uint64_t vmm_alloc_block(uint64_t num_pages);

// Free a previously allocated block
// Returns 0 on success, negative error code on failure
int vmm_free_block(uint64_t address, uint64_t num_pages);

#endif