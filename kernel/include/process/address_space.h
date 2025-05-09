/*
 * stage3 - Process address space handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_PROCESS_ADDRESS_SPACE_H
#define __ANOS_KERNEL_ARCH_X86_64_PROCESS_ADDRESS_SPACE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uintptr_t start;
    uint64_t len_bytes;
} AddressSpaceRegion;

// This **must** be called **after** basic kernel init is complete, and
// fixed areas are set up and the PMM and VMM initialized.
//
// It will empty PDPTs for all of kernel space (except the recursive mapping
// and the reserved mapping immediately after). This "wastes" about a MiB
// of physical RAM, but does mean that kernel space mappings in all processes
// will stay consistent with no additional work needed because every
// address space we create from here on out will start with a copy of
// the kernel space mappings from this PML4...
bool address_space_init(void);

/*
 * Create a new address space, based on the current one. This will
 *
 * * Allocate a new address space
 * * Copy all kernel PDPTs into it
 * * Map the space covered by `regions` as COW shared
 * * Allocate pages to cover `init_stack_len` bytes and map it at `init_stack_vaddr`
 * * Set up initial values at the bottom of the stack
 * 
 * Currently, on failure, this will leak some memory - that'll be fixed once
 * I put a proper address space destroy function in.
 * 
 * Returns the physical address of the new PML4.
 */
uintptr_t address_space_create(uintptr_t init_stack_vaddr,
                               uint64_t init_stack_len, uint64_t region_count,
                               AddressSpaceRegion regions[],
                               uint64_t stack_value_count,
                               uint64_t *stack_values);

#endif //__ANOS_KERNEL_ARCH_X86_64_PROCESS_ADDRESS_SPACE_H
