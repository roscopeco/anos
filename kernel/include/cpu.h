/*
 * stage3 - Generic CPU driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_CPU_H
#define __ANOS_KERNEL_CPU_H

#ifdef ARCH_X86_64
#include "x86_64/kdrivers/cpu.h"
#elifdef ARCH_RISCV64
#include "riscv64/kdrivers/cpu.h"
#else
#error Need to define an arch-specific cpu.h
#endif

#endif