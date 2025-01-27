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

uint64_t cpu_read_msr(uint32_t msr);
uint64_t rdtsc(void);

void cpu_tsc_delay(uint64_t cycles);
void cpu_tsc_mdelay(int n);
void cpu_tsc_udelay(int n);

void cpu_debug_info(void);

#endif //__ANOS_KERNEL_DRIVERS_CPU_H