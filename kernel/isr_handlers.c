/*
 * stage3 - ISR handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */
#include <stdbool.h>
#include <stdint.h>

#include "acpitables.h"
#include "debugprint.h"
#include "general_protection_fault.h"
#include "machine.h"
#include "pagefault.h"
#include "printhex.h"

/*
 * Basic debug handler for exceptions with no error code.
 *
 * Called from the relevant ISR dispatchers.
 */
static void debug_exception_nc(uint8_t vector, uint64_t origin_addr) {
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr(": Unhandled exception (");
    printhex8(vector, debugchar);
    debugstr(")\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);
    debugstr("\nHalting...");
    halt_and_catch_fire();
}

/*
 * Basic debug handler for exceptions with an error code.
 *
 * Called from the relevant ISR dispatchers.
 */
static void debug_exception_wc(uint8_t vector, uint64_t code,
                               uint64_t origin_addr) {
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr("         : Unhandled exception (");
    printhex8(vector, debugchar);
    debugstr(")\nCode          : ");
    printhex64(code, debugchar);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);

    debugstr("\nHalting...");
    halt_and_catch_fire();
}

/*
 * Actual handler for exceptions with no code.
 *
 * For now, just calls debug handler, above.
 */
void handle_exception_nc(uint8_t vector, uint64_t origin_addr) {
    debug_exception_nc(vector, origin_addr);
}

/*
 * Actual handler for exceptions with error code.
 *
 * For now, just calls debug handler, above.
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
        debug_exception_wc(vector, code, origin_addr);
    }
}

void handle_unknown_interrupt() {
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr(": Unhandled interrupt!\n");
    debugstr("Halting...");
    halt_and_catch_fire();
}
