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
#include "platform/acpi/acpitables.h"
#include "printhex.h"
#include "x86_64/general_protection_fault.h"

/*
 * Actual handler for exceptions with no code.
 *
 * For now, just panics.
 */
void handle_exception_nc(const uint8_t vector, const uint64_t origin_addr) {
    panic_exception_no_code(vector, origin_addr);
}

/*
 * Actual handler for exceptions with error code.
 *
 * For now, just panics.
 */
void handle_exception_wc(const uint8_t vector, const uint64_t code,
                         const uint64_t origin_addr) {
    uint64_t fault_addr;

    switch (vector) {
    case 0x0e:
        // page fault - grab fault address from cr2...
        // This should only happen during early boot, we replace the
        // handler once tasking is up...
        __asm__ volatile("movq %%cr2,%0\n\t" : "=r"(fault_addr));
        early_page_fault_handler(code, fault_addr, origin_addr);
        break;
    case 0x0d:
        handle_general_protection_fault(code, origin_addr);
        break;
    default:
        panic_exception_with_code(vector, code, origin_addr);
    }
}

void handle_unknown_interrupt() { panic("Unhandled interrupt!"); }
