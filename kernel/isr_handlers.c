/*
 * stage3 - ISR handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "debugprint.h"
#include "printhex.h"
#include "machine.h"

/*
 * Basic debug handler for exceptions with no error code.
 *
 * Called from the relevant ISR dispatchers.
 */
void debug_interrupt_nc(uint8_t vector) {
    debugstr("PANIC: Unhandled exception ");
    printhex8(vector, debugchar);
    debugstr("\nHalting...");
    halt_and_catch_fire();
}

/*
 * Basic debug handler for exceptions with an error code.
 *
 * Called from the relevant ISR dispatchers.
 */
void debug_interrupt_wc(uint8_t vector, uint64_t code) {
    debugstr("PANIC: Unhandled exception ");
    printhex8(vector, debugchar);
    debugstr(": code = ");
    printhex64(code, debugchar);
    debugstr("\nHalting...");
    halt_and_catch_fire();
}