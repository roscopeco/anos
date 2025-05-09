/*
 * stage3 - General Protection fault handler
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_GENERAL_PROTECTION_FAULT_H
#define __ANOS_KERNEL_ARCH_X86_64_GENERAL_PROTECTION_FAULT_H

#include <stdint.h>

void handle_general_protection_fault(uint64_t code, uintptr_t origin_addr);

#endif //__ANOS_KERNEL_ARCH_X86_64_GENERAL_PROTECTION_FAULT_H