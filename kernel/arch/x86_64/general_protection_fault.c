/*
 * stage3 - The GPF handler
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "isr_frame.h"
#include "panic.h"

void handle_general_protection_fault(const uint64_t code, const uintptr_t origin_addr,
                                     const IsrStackFrameWithCode *stack_frame) {
    panic_general_protection_fault(code, origin_addr, stack_frame->registers.rbp);
}
