/*
 * stage3 - Kernel physical memory manager config
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_PMM_CONFIG_H
#define __ANOS_KERNEL_ARCH_X86_64_PMM_CONFIG_H

#include <pmm/pagealloc.h>

// This is the static virtual address region (128GB from this base)
// that is reserved for PMM structures and stack.
#ifndef STATIC_PMM_VREGION
#define STATIC_PMM_VREGION ((void *)0xFFFFFF8000000000)
#endif

// The base address of the physical region the pmm allocator should manage.
#ifndef PMM_PHYS_BASE
#define PMM_PHYS_BASE 0x200000
#endif

extern MemoryRegion *physical_region;

#endif //__ANOS_KERNEL_ARCH_X86_64_PMM_CONFIG_H