/*
 * RISC-V Task switch interface
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * 
 * (TODO I don't think we actually need this...)
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_RISCV64_TASK_SWITCH_H
#define __ANOS_KERNEL_ARCH_RISCV64_TASK_SWITCH_H

#include "task.h"
#include <stdint.h>

// Function to perform task switch
// This is implemented in assembly in task_switch.S
void task_do_switch(Task *next);

// Constants for task switch
#define TASK_SWITCH_STACK_SIZE 256 // Size of stack frame for saved registers
#define TASK_SWITCH_FP_OFFSET                                                  \
    184 // Offset to start of FP registers in stack frame

#endif // __ANOS_KERNEL_ARCH_RISCV64_TASK_SWITCH_H