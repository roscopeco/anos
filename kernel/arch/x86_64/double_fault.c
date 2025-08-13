/*
 * stage3 - The #DF handler
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "panic.h"

void double_fault_handler(const uintptr_t origin_addr) {
    panic_double_fault(origin_addr);
}
