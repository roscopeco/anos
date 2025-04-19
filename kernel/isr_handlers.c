/*
 * stage3 - ISR handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */
#include <stdbool.h>
#include <stdint.h>

#include "debugprint.h"
#include "machine.h"
#include "pagefault.h"
#include "panic.h"
#include "printhex.h"
#include "x86_64/acpitables.h"
#include "x86_64/general_protection_fault.h"

/*
 * Actual handler for exceptions with no code.
 *
 * For now, just panics.
 */
void handle_exception_nc(uint8_t vector, uint64_t origin_addr) {
    panic_exception_no_code(vector, origin_addr);
}

/*
 * Actual handler for exceptions with error code.
 *
 * For now, just panics.
 */
void handle_exception_wc(uint8_t vector, uint64_t code, uint64_t origin_addr) {
    uint64_t fault_addr;

    switch (vector) {
    case 0x0e:
        // page fault - grab fault address from cr2...
        __asm__ volatile("movq %%cr2,%0\n\t" : "=r"(fault_addr));
        handle_page_fault(code, fault_addr, origin_addr);
        break;
    case 0x0d:
        handle_general_protection_fault(code, origin_addr);
        break;
    default:
        panic_exception_with_code(vector, code, origin_addr);
    }
}

void handle_unknown_interrupt() { panic("Unhandled interrupt!"); }
