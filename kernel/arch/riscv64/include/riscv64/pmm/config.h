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
#ifdef RISCV_SV48
#define STATIC_PMM_VREGION ((void *)0xffffff8000000000)
#elifdef RISCV_SV39
#define STATIC_PMM_VREGION ((void *)0xffffffff00000000)
#else
#error RISC-V paging mode invalid or not defined
#endif
#endif

#ifndef ANOS_NO_DECL_REGIONS
extern MemoryRegion *physical_region;
#endif

#endif //__ANOS_KERNEL_PMM_CONFIG_H