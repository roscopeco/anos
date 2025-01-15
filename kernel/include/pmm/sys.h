/*
 * stage3 - Physical memory subsystem routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PMM_SYS_H
#define __ANOS_KERNEL_PMM_SYS_H

#include <stdint.h>

uintptr_t get_pagetable_root();
void set_pagetable_root(uintptr_t new_root);

#endif //__ANOS_KERNEL_PMM_SYS_H
