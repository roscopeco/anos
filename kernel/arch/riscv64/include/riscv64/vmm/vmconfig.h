/*
 * stage3 - Virtual memory configuration for RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV_VM_CONFIG_H
#define __ANOS_KERNEL_ARCH_RISCV_VM_CONFIG_H

#include <stdint.h>

// Page size constants
#define PAGE_SIZE ((uintptr_t)(0x1000))            // 4KB
#define MEGA_PAGE_SIZE ((uintptr_t)(0x200000))     // 2MB
#define GIGA_PAGE_SIZE ((uintptr_t)(0x40000000))   // 1GB
#define TERA_PAGE_SIZE ((uintptr_t)(0x8000000000)) // 512GB

#define VM_PAGE_SIZE ((PAGE_SIZE))

#endif //__ANOS_KERNEL_ARCH_RISCV_VM_CONFIG_H