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

#if defined __x86_64__
#include "x86_64/vmm/vmconfig.h"
#elif defined __riscv
#include "riscv64/vmm/vmconfig.h"
#else
#error Undefined or unsupported architecture
#endif

#define VM_PAGE_LINEAR_SHIFT ((__builtin_ctz(VM_PAGE_SIZE)))

static_assert(VM_PAGE_SIZE >> VM_PAGE_LINEAR_SHIFT == 1,
              "Page shift not constant");

#endif //__ANOS_KERNEL_VM_CONFIG_H