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

void handle_page_fault(uint64_t code, uintptr_t fault_addr,
                       uintptr_t origin_addr);

#endif //__ANOS_KERNEL_PAGEFAULT_H