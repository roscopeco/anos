/*
 * stage3 - The page fault handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "machine.h"
#include "pmm/pagealloc.h"
#include "printhex.h"
#include "vmm/vmmapper.h"

#define IS_KERNEL_CODE(addr) (((addr & 0xFFFFFFFF80000000) != 0))

#ifdef DEBUG_PAGE_FAULT
#define C_DEBUGSTR debugstr
#define C_DEBUGATTR debugattr
#define C_PRINTHEX64 printhex64
#else
#define C_DEBUGSTR(...)
#define C_DEBUGATTR(...)
#define C_PRINTHEX64(...)
#endif

static inline void debug_page_fault_code(uint8_t code) {
    C_DEBUGSTR(" [");
    C_DEBUGSTR(code & 0x8000 ? "SGX|" : "sgx|");
    C_DEBUGSTR(code & 0x40 ? "SS|" : "ss|");
    C_DEBUGSTR(code & 0x20 ? "PK|" : "pk|");
    C_DEBUGSTR(code & 0x10 ? "I|" : "i|");
    C_DEBUGSTR(code & 0x8 ? "R|" : "r|");
    C_DEBUGSTR(code & 0x4 ? "U|" : "u|");
    C_DEBUGSTR(code & 0x2 ? "W|" : "w|");
    C_DEBUGSTR(code & 0x1 ? "P" : "p");
    C_DEBUGSTR("]");
}

extern MemoryRegion *physical_region;

static inline void debug_page_fault(uint64_t code, uintptr_t fault_addr,
                                    uintptr_t origin_addr) {
    if (fault_addr > 0xFFFFFFFF80400000 && fault_addr < 0xFFFFFFFF81000000) {
        C_DEBUGATTR(0x0E);
        C_DEBUGSTR("## Page fault in kernel vmspace; Will alloc & map a "
                   "page...\n");

        // Allocate a page
        uint64_t page = page_alloc(physical_region);

        C_DEBUGSTR("Mapped page for ");
        C_PRINTHEX64(fault_addr, debugchar);
        C_DEBUGSTR(" at phys ");
        C_PRINTHEX64(page, debugchar);
        C_DEBUGSTR("\n");

        // Map the page
        vmm_map_page(STATIC_PML4, fault_addr, page, PRESENT | WRITE);
        C_DEBUGATTR(0x07);

        return;
    }

    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);
    debugstr("         : Unhandled page fault (0x0e)");
    debugstr("\nCode          : ");
    printhex64(code, debugchar);
    debug_page_fault_code(code);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);
    debugstr(IS_KERNEL_CODE(origin_addr) ? " [KERNEL]" : " [NON-KERNEL]");
    debugstr("\nFault addr    : ");
    printhex64(fault_addr, debugchar);

    halt_and_catch_fire();
}

void handle_page_fault(uint64_t code, uint64_t fault_addr,
                       uint64_t origin_addr) {
    debug_page_fault(code, fault_addr, origin_addr);
}
