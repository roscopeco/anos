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
#include <stddef.h>
#include <stdint.h>

// We allow up to 33 pages (128KiB) at the top of the stack for initial
// arg values etc.
#define INIT_STACK_ARG_PAGES_COUNT ((33))
#define MAX_STACK_VALUE_COUNT                                                  \
    ((INIT_STACK_ARG_PAGES_COUNT - 1) * VM_PAGE_SIZE / (sizeof(uintptr_t)))

typedef struct {
    uintptr_t start;
    uint64_t len_bytes;
} AddressSpaceRegion;

// This **must** be called **after** basic kernel init is complete, and
// fixed areas are set up and the PMM and VMM initialized.
//
// It will create empty PDPTs for all of kernel space (except the reserved
// virtual mapping areas and other areas that are already set up by the
// time this runs).
//
// This "wastes" about a MiB of physical RAM, but does mean that kernel
// space mappings in all processes will stay consistent with no additional
// work needed because every address space we create from here on out will
// start with a copy of the kernel space mappings from this PML4...
bool address_space_init(void);

/*
 * Create a new address space, based on the current one. This will:
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
                               size_t init_stack_len, int region_count,
                               AddressSpaceRegion regions[],
                               int stack_value_count,
                               const uint64_t *stack_values);

#endif //__ANOS_KERNEL_ARCH_X86_64_PROCESS_ADDRESS_SPACE_H
