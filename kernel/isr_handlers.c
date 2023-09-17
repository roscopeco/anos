/*
 * stage3 - ISR handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */
#include <stdint.h>

#include "debugprint.h"
#include "printhex.h"
#include "machine.h"
#include "pagefault.h"

/*
 * Basic debug handler for exceptions with no error code.
 *
 * Called from the relevant ISR dispatchers.
 */
static void debug_interrupt_nc(uint8_t vector, uint64_t origin_addr) {
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
static void debug_interrupt_wc(uint8_t vector, uint64_t code, uint64_t origin_addr) {
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
void handle_interrupt_nc(uint8_t vector, uint64_t origin_addr) {
    debug_interrupt_nc(vector, origin_addr);
}

/*
 * Actual handler for exceptions with error code.
 *
 * For now, just calls debug handler, above.
 */
void handle_interrupt_wc(uint8_t vector, uint64_t code, uint64_t origin_addr) {
    if (vector == 0x0e) {
        // page fault - grab fault address from cr2...
        uint64_t fault_addr;
        asm("movq %%cr2,%0\n\t" : "=r"(fault_addr));    
        handle_page_fault(code, fault_addr, origin_addr);
    } else {
        debug_interrupt_wc(vector, code, origin_addr);
    }
}
