/*
 * stage3 - Process address space handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PROCESS_ADDRESS_SPACE_H
#define __ANOS_KERNEL_PROCESS_ADDRESS_SPACE_H

#include <stdbool.h>
#include <stdint.h>

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

uintptr_t address_space_create(void);

#endif //__ANOS_KERNEL_PROCESS_ADDRESS_SPACE_H
