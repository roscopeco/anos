/*
 * stage3 - Kernel Panic handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 * 
 * TODO this needs to report CPU number.
 *      It should probably also have a dedicated stack...
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "machine.h"
#include "printdec.h"
#include "printhex.h"
#include "smp/state.h"
#include "spinlock.h"

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

static SpinLock panic_lock;

noreturn void panic(const char *msg) {
    disable_interrupts();
    spinlock_lock(&panic_lock);

    debugattr(0x0C);
    debugstr("\n\n###############################################\n");
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr("         : ");
    debugstr((char *)msg);
    debugstr(" - ");

    debugstr("\nHalting...");

    spinlock_unlock(&panic_lock);
    halt_and_catch_fire();
}

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

noreturn void panic_page_fault(uintptr_t origin_addr, uintptr_t fault_addr,
                               uint64_t code) {
    disable_interrupts();
    spinlock_lock(&panic_lock);

    debugattr(0x0C);
    debugstr("\n\n###############################################\n");
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);
    debugstr("         : Unhandled page fault (0x0e)");

    // PerCPUState *cpu_state = state_get_per_cpu();
    // debugstr("\nCPU           : ");
    // printdec(cpu_state->cpu_id, debugchar);

    debugstr("\nCode          : ");
    printhex64(code, debugchar);
    debug_page_fault_code(code);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);
    debugstr(IS_KERNEL_CODE(origin_addr) ? " [KERNEL]" : " [NON-KERNEL]");
    debugstr("\nFault addr    : ");
    printhex64(fault_addr, debugchar);
    debugstr("\nHalting...");

    spinlock_unlock(&panic_lock);
    halt_and_catch_fire();
}

noreturn void panic_general_protection_fault(uint64_t code,
                                             uintptr_t origin_addr) {
    disable_interrupts();
    spinlock_lock(&panic_lock);

    debugattr(0x0C);
    debugstr("\n\n###############################################\n");
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr("         : General Protection Fault (0x0d)");

    PerCPUState *cpu_state = state_get_per_cpu();
    debugstr("\nCPU           : ");
    printdec(cpu_state->cpu_id, debugchar);

    debugstr("\nCode          : ");
    printhex64(code, debugchar);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);

    debugstr("\nHalting...");

    spinlock_unlock(&panic_lock);
    halt_and_catch_fire();
}

noreturn void panic_exception_with_code(uint8_t vector, uint64_t code,
                                        uintptr_t origin_addr) {
    disable_interrupts();
    spinlock_lock(&panic_lock);

    debugattr(0x0C);
    debugstr("\n\n###############################################\n");
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

    spinlock_unlock(&panic_lock);
    halt_and_catch_fire();
}

noreturn void panic_exception_no_code(uint8_t vector, uintptr_t origin_addr) {
    disable_interrupts();
    spinlock_lock(&panic_lock);

    debugattr(0x0C);
    debugstr("\n\n###############################################\n");
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);

    debugstr(": Unhandled exception (");
    printhex8(vector, debugchar);
    debugstr(")\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);
    debugstr("\nHalting...");

    spinlock_unlock(&panic_lock);
    halt_and_catch_fire();
}