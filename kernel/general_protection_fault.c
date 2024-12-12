/*
 * stage3 - The GPF handler
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "machine.h"
#include "printhex.h"

#ifdef DEBUG_PAGE_FAULT
#define C_DEBUGSTR debugstr
#define C_DEBUGATTR debugattr
#define C_PRINTHEX64 printhex64
#else
#define C_DEBUGSTR(...)
#define C_DEBUGATTR(...)
#define C_PRINTHEX64(...)
#endif

static inline void debug_general_protection_fault(uint64_t code,
                                                  uintptr_t origin_addr) {
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr("         : General Protection Fault (0x0d)\nCode          : ");
    printhex64(code, debugchar);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);

    debugstr("\nHalting...");
    halt_and_catch_fire();
}

void handle_general_protection_fault(uint64_t code, uintptr_t origin_addr) {
    debug_general_protection_fault(code, origin_addr);
}
