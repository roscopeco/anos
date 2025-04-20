/*
 * stage3 - Kernel physical memory manager config (RISC-V)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_PMM_CONFIG_H
#define __ANOS_KERNEL_ARCH_RISCV64_PMM_CONFIG_H

#include <pmm/pagealloc.h>

// This is the static virtual address region (128GB from this base)
// that is reserved for PMM structures and stack.
#ifndef STATIC_PMM_VREGION
#define STATIC_PMM_VREGION ((void *)0xFFFFFF8000000000)
#endif

#ifndef ANOS_NO_DECL_REGIONS
extern MemoryRegion *physical_region;
#endif

#endif //__ANOS_KERNEL_PMM_CONFIG_H