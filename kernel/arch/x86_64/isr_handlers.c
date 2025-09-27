/*
 * stage3 - ISR handlers for x86_64
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "machine.h"
#include "pagefault.h"
#include "panic.h"

#include "x86_64/general_protection_fault.h"
#include "x86_64/isr_frame.h"

/*
 * Actual handler for exceptions with no code.
 *
 * For now, just panics.
 */
void handle_exception_nc(const uint8_t vector, const uint64_t origin_addr, const IsrStackFrameNoCode *stack_frame) {
    panic_exception_no_code(vector, origin_addr, stack_frame->registers.rbp);
}

/*
 * Actual handler for exceptions with error code.
 *
 * Handles very early #PF (which gets replaced after system is up) and also #GP (which just panics)
 */
void handle_exception_wc(const uint8_t vector, const uint64_t code, const uint64_t origin_addr,
                         IsrStackFrameWithCode *stack_frame) {
    uint64_t fault_addr;

    switch (vector) {
    case 0x0e:
        // page fault - grab fault address from cr2...
        // This should only happen during early boot, we replace the
        // handler once tasking is up...
        __asm__ volatile("movq %%cr2,%0\n\t" : "=r"(fault_addr));
        early_page_fault_handler(code, fault_addr, origin_addr, stack_frame);
        break;
    case 0x0d:
        handle_general_protection_fault(code, origin_addr, stack_frame);
        break;
    default:
        panic_exception_with_code(vector, code, origin_addr, stack_frame->registers.rbp);
    }
}

void handle_unknown_interrupt() { panic("Unhandled interrupt!"); }
