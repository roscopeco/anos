/*
 * stage3 - Kernel CPU driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DRIVERS_CPU_H
#define __ANOS_KERNEL_DRIVERS_CPU_H

#include <stdbool.h>
#include <stdint.h>

bool cpu_init_this(void);
void cpu_debug_info(void);
uint64_t cpu_read_msr(uint32_t msr);

#endif //__ANOS_KERNEL_DRIVERS_CPU_H