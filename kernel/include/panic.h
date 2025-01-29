/*
 * stage3 - Kernel Panic handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PANIC_H
#define __ANOS_KERNEL_PANIC_H

#include <stdint.h>
#include <stdnoreturn.h>

// Just panic...
noreturn void panic(const char *msg);

// Generic exception panics
noreturn void panic_exception_with_code(uint8_t vector, uint64_t code,
                                        uintptr_t origin_addr);
noreturn void panic_exception_no_code(uint8_t vector, uintptr_t origin_addr);

// Specific exception panics
noreturn void panic_page_fault(uintptr_t origin_addr, uintptr_t fault_addr,
                               uint64_t code);
noreturn void panic_general_protection_fault(uint64_t code,
                                             uintptr_t origin_addr);

#endif //__ANOS_KERNEL_PANIC_H