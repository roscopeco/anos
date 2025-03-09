/*
 * stage3 - Virtual memory configuration
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_VM_CONFIG_H
#define __ANOS_KERNEL_VM_CONFIG_H

#include "anos_assert.h"

#define VM_KERNEL_SPACE_START ((0xffff800000000000))
#define VM_PAGE_SIZE ((0x1000))
#define VM_PAGE_LINEAR_SHIFT ((__builtin_ctz(VM_PAGE_SIZE)))

static_assert(VM_PAGE_SIZE >> VM_PAGE_LINEAR_SHIFT == 1,
              "Page shift not constant");

#endif //__ANOS_KERNEL_VM_CONFIG_H