/*
 * stage3 - Kernel page table initialisation (x86_64)
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_INIT_PAGETABLES_H
#define __ANOS_KERNEL_ARCH_X86_64_INIT_PAGETABLES_H

// Returns a virtual address for the PML4...
//
uint64_t *pagetables_init(void);

#endif //__ANOS_KERNEL_ARCH_X86_64_INIT_PAGETABLES_H
