/*
 * stage3 - Abstract timer interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_CPUID_H
#define __ANOS_KERNEL_CPUID_H

#include <stdbool.h>
#include <stdint.h>

// TODO this obviously can't be global once we're spinning up
//      more CPUs, but it's okay for now...
//
extern const char __cpuid_vendor[13];
#define cpuid_vendor (((char *)__cpuid_vendor))

void init_cpuid(void);

bool cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
           uint32_t *edx);

#endif //__ANOS_KERNEL_CPUID_H
