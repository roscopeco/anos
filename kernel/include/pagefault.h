/*
 * stage3 - Page fault handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_PAGEFAULT_H
#define __ANOS_KERNEL_PAGEFAULT_H

#include <stdint.h>

void pagefault_notify_smp_started(void);

void early_page_fault_handler(uint64_t code, uint64_t fault_addr, uint64_t origin_addr);

void page_fault_handler(uint64_t code, uintptr_t fault_addr, uintptr_t origin_addr);

#endif //__ANOS_KERNEL_PAGEFAULT_H