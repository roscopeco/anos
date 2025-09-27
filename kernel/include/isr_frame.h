/*
* stage3 - ISR stack frame (multi-arch)
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ISR_FRAME_H
#define __ANOS_KERNEL_ISR_FRAME_H

#ifdef ARCH_X86_64
#include "x86_64/isr_frame.h"
#elifdef ARCH_RISCV64
#include "riscv64/isr_frame.h"
#else
#warning "Unsupported architecture in isr_frame.h"
#endif

#endif