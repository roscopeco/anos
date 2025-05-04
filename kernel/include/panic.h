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

#define PANIC_IPI_VECTOR ((0x02)) // Use NMI for Panic IPI

#ifdef UNIT_TESTS
#define NORETURN_EXCEPT_TESTS
#else
#define NORETURN_EXCEPT_TESTS noreturn
#endif

// This must be called once SMP and the scheduler is up,
// to notify the panic subsystem that panics from now on
// must support SMP and can use CPU-local data.
void panic_notify_smp_started(void);

// Just panic...
#define panic(msg) panic_sloc(msg, __FILE__, __LINE__)
NORETURN_EXCEPT_TESTS void panic_sloc(const char *msg, const char *filename,
                                      const uint64_t line);

// Generic exception panics
#define panic_exception_with_code(vector, code, origin_addr)                   \
    panic_exception_with_code_sloc(vector, code, origin_addr, __FILE__,        \
                                   __LINE__)
NORETURN_EXCEPT_TESTS void panic_exception_with_code_sloc(uint8_t vector,
                                                          uint64_t code,
                                                          uintptr_t origin_addr,
                                                          const char *filename,
                                                          const uint64_t line);

#define panic_exception_no_code(vector, origin_addr)                           \
    panic_exception_no_code_sloc(vector, origin_addr, __FILE__, __LINE__)
NORETURN_EXCEPT_TESTS void panic_exception_no_code_sloc(uint8_t vector,
                                                        uintptr_t origin_addr,
                                                        const char *filename,
                                                        const uint64_t line);

// Specific exception panics
#define panic_page_fault(origin_addr, fault_addr, code)                        \
    panic_page_fault_sloc(origin_addr, fault_addr, code, __FILE__, __LINE__)
NORETURN_EXCEPT_TESTS void
panic_page_fault_sloc(uintptr_t origin_addr, uintptr_t fault_addr,
                      uint64_t code, const char *filename, const uint64_t line);

#define panic_general_protection_fault(code, origin_addr)                      \
    panic_general_protection_fault_sloc(code, origin_addr, __FILE__, __LINE__)
NORETURN_EXCEPT_TESTS void
panic_general_protection_fault_sloc(uint64_t code, uintptr_t origin_addr,
                                    const char *filename, const uint64_t line);

#endif //__ANOS_KERNEL_PANIC_H