/*
 * stage3 - Common code used by various entry points
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_ENTRYPOINTS_COMMON_H
#define __ANOS_KERNEL_ENTRYPOINTS_COMMON_H

void init_kernel_gdt(void);
void install_interrupts();
void banner(void);

#endif //__ANOS_KERNEL_ENTRYPOINTS_COMMON_H