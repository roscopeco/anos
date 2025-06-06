/*
 * stage3 - Mock kernel CPU driver for tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_MOCK_CPU_H
#define __ANOS_KERNEL_MOCK_CPU_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "x86_64/gdt.h"
#include "x86_64/interrupts.h"

#define MSR_FSBase ((0xC0000100))
#define MSR_GSBase ((0xC0000101))
#define MSR_KernelGSBase ((0xC0000102))

#define CPU_TSS_ENTRY_SIZE_MULT ((2))

#ifdef MUNIT_H
extern uint64_t mock_cpu_msr_value;
extern uint64_t mock_invlpg_count;
#endif

#include "x86_64/kdrivers/cpu.h"

#endif //__ANOS_KERNEL_MOCK_CPU_H