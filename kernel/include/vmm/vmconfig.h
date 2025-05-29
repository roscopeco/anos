/*
 * stage3 - Virtual memory configuration
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_VM_CONFIG_H
#define __ANOS_KERNEL_VM_CONFIG_H

#include "anos_assert.h"

#define VM_KERNEL_SPACE_START ((0xffff800000000000ULL))

#ifdef ARCH_X86_64
#include "x86_64/vmm/vmconfig.h"
#elifdef ARCH_RISCV64
#include "riscv64/vmm/vmconfig.h"
#else
#error Undefined or unsupported architecture
#endif

#define VM_PAGE_LINEAR_SHIFT ((__builtin_ctz(VM_PAGE_SIZE)))

#define MAX_PHYS_ADDR (((size_t)127 * 1024 * 1024 * 1024 * 1024)) // 127 TiB

static_assert(VM_PAGE_SIZE >> VM_PAGE_LINEAR_SHIFT == 1,
              "Page shift not constant");

#endif //__ANOS_KERNEL_VM_CONFIG_H