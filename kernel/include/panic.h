/*
 * stage3 - Kernel Panic handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PANIC_H
#define __ANOS_KERNEL_PANIC_H

#include <stdint.h>
#include <stdnoreturn.h>

// Just panic...
#define panic(msg) panic_sloc(msg, __FILE__, __LINE__)
noreturn void panic_sloc(const char *msg, const char *filename,
                         const uint64_t line);

// Generic exception panics
#define panic_exception_with_code(vector, code, origin_addr)                   \
    panic_exception_with_code_sloc(vector, code, origin_addr, __FILE__,        \
                                   __LINE__)
noreturn void panic_exception_with_code_sloc(uint8_t vector, uint64_t code,
                                             uintptr_t origin_addr,
                                             const char *filename,
                                             const uint64_t line);

#define panic_exception_no_code(vector, origin_addr)                           \
    panic_exception_no_code_sloc(vector, origin_addr, __FILE__, __LINE__)
noreturn void panic_exception_no_code_sloc(uint8_t vector,
                                           uintptr_t origin_addr,
                                           const char *filename,
                                           const uint64_t line);

// Specific exception panics
#define panic_page_fault(origin_addr, fault_addr, code)                        \
    panic_page_fault_sloc(origin_addr, fault_addr, code, __FILE__, __LINE__)
noreturn void panic_page_fault_sloc(uintptr_t origin_addr, uintptr_t fault_addr,
                                    uint64_t code, const char *filename,
                                    const uint64_t line);

#define panic_general_protection_fault(code, origin_addr)                      \
    panic_general_protection_fault_sloc(code, origin_addr, __FILE__, __LINE__)
noreturn void panic_general_protection_fault_sloc(uint64_t code,
                                                  uintptr_t origin_addr,
                                                  const char *filename,
                                                  const uint64_t line);

#endif //__ANOS_KERNEL_PANIC_H