/*
 * stage3 - Kernel page table initialisation (RISC-V)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_INIT_PAGETABLES_H
#define __ANOS_KERNEL_ARCH_RISCV64_INIT_PAGETABLES_H

#include <stdint.h>

void pagetables_init(uint64_t *pml4, uint64_t *pdpt, uint64_t *pd);

#endif //__ANOS_KERNEL_ARCH_RISCV64_INIT_PAGETABLES_H
