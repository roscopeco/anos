/*
 * stage3 - Common code used by various entry points
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_ENTRYPOINTS_COMMON_H
#define __ANOS_KERNEL_ARCH_X86_64_ENTRYPOINTS_COMMON_H

void init_kernel_gdt(void);
void install_interrupts();

#endif //__ANOS_KERNEL_ARCH_X86_64_ENTRYPOINTS_COMMON_H