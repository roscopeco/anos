/*
 * stage3 - Kernel Panic handler
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 * 
 * TODO this needs to report CPU number.
 *      It should probably also have a dedicated stack...
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "kprintf.h"
#include "machine.h"
#include "printdec.h"
#include "printhex.h"
#include "sched.h"
#include "smp/ipwi.h"
#include "smp/state.h"
#include "spinlock.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #notknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#define IS_KERNEL_CODE(addr) (((addr & 0xFFFFFFFF80000000) != 0))

#define STACK_TRACE_MAX ((20))

static SpinLock panic_lock;
static bool smp_is_up;

#ifdef ARCH_X86_64
static void print_stack_trace() {
    uintptr_t *ebp, *eip;
    uint8_t count = 0;

    __asm__ volatile("mov %%rbp, %0" : "=r"(ebp));

    debugattr(0x0C);
    debugstr("\n\nExecution trace:\n");

    while (ebp && (count++ < STACK_TRACE_MAX)) {
        eip = ebp + 1;
        debugattr(0x08);
        debugstr("   [");
        debugattr(0x07);
        printhex64(*eip, debugchar);
        debugattr(0x08);
        debugstr("] ");
        debugattr(0xf);
        debugstr("<unknown>\n"); // TODO lookup symbol
        ebp = (uintptr_t *)*ebp;
    }
}
#else
#define print_stack_trace()
#endif

static inline void print_header_vec(char *msg, uint8_t vector) {
    debugattr(0x0C);
    debugstr("\n\n###################################");
    debugattr(0x04);
    debugstr("[");
    debugattr(0x08);
    debugstr(VERSION);
    debugattr(0x04);
    debugstr("]");
    debugattr(0x0C);
    debugstr("###################################\n");
    debugattr(0xC0);
    debugstr("PANIC");
    debugattr(0x0C);
    debugstr("      : ");
    debugattr(0x0F);
    debugstr(msg);
    debugattr(0x08);
    debugstr(" (");
    debugattr(0x07);
    printhex8(vector, debugchar);
    debugattr(0x08);
    debugstr(")");
}

static inline void print_header_no_vec(const char *msg) {
    debugattr(0x0C);
    debugstr("\n\n###################################");
    debugattr(0x04);
    debugstr("[");
    debugattr(0x08);
    debugstr(VERSION);
    debugattr(0x04);
    debugstr("]");
    debugattr(0x0C);
    debugstr("###################################\n");
    debugattr(0xC0);
    debugstr("PANIC");
    debugattr(0x0C);
    debugstr("      : ");
    debugattr(0x0F);
    debugstr((char *)msg);
}

static inline void print_loc(const char *filename, const uint64_t line) {
    debugattr(0x0C);
    debugstr("\n         @ : ");
    debugattr(0x07);
    debugstr((char *)filename);
    debugattr(0x08);
    debugstr(":");
    debugattr(0x07);
    printdec(line, debugchar);
}

static inline void print_cpu(void) {
    if (smp_is_up) {
        PerCPUState *state = state_get_for_this_cpu();

        debugattr(0x0C);
        debugstr("\nCPU        : ");
        debugattr(0x07);

        if (state) {
            printdec(state->cpu_id, debugchar);
        } else {
            debugstr("<unknown>");
        }
    }
}
static inline void print_code(uint64_t code) {
    debugattr(0x0C);
    debugstr("\nCode       : ");
    debugattr(0x07);
    printhex64(code, debugchar);
}

static inline void print_page_fault_code(uint8_t code) {
    debugattr(0x0C);
    debugstr("\n         = : ");
    debugattr(0x7);
    debugstr("[");

    if (code & 0x8000) {
        debugattr(0xa);
        debugstr("SGX");
    } else {
        debugattr(0x8);
        debugstr("sgx");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x40) {
        debugattr(0xa);
        debugstr("SS");
    } else {
        debugattr(0x8);
        debugstr("ss");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x20) {
        debugattr(0xa);
        debugstr("PK");
    } else {
        debugattr(0x8);
        debugstr("pk");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x10) {
        debugattr(0xa);
        debugstr("I");
    } else {
        debugattr(0x8);
        debugstr("i");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x08) {
        debugattr(0xa);
        debugstr("R");
    } else {
        debugattr(0x8);
        debugstr("r");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x04) {
        debugattr(0xa);
        debugstr("U");
    } else {
        debugattr(0x8);
        debugstr("u");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x02) {
        debugattr(0xa);
        debugstr("W");
    } else {
        debugattr(0x8);
        debugstr("w");
    }
    debugattr(0x7);
    debugstr("|");

    if (code & 0x01) {
        debugattr(0xa);
        debugstr("P");
    } else {
        debugattr(0x8);
        debugstr("p");
    }
    debugattr(0x7);
    debugstr("]");
}

static inline void print_origin_ip(uintptr_t origin_addr) {
    debugattr(0x0C);
    debugstr("\nOrigin IP  : ");
    debugattr(0x07);
    printhex64(origin_addr, debugchar);
    debugattr(0x08);
    debugstr(" [");
    debugattr(0x07);
    debugstr(IS_KERNEL_CODE(origin_addr) ? "KERNEL" : "NON-KERNEL");
    debugattr(0x08);
    debugstr("]");
}

static inline void print_fault_addr(uint64_t fault_addr) {
    debugattr(0x0C);
    debugstr("\nFault addr : ");
    debugattr(0x07);
    printhex64(fault_addr, debugchar);
}

static inline void print_footer(void) {
    debugattr(0x0C);
    debugstr("\n##################################");
    debugattr(0x04);
    debugstr(" Halting... ");
    debugattr(0x0C);
    debugstr("#################################\n");
    debugstr("\n");
    debugattr(0x07);
}

static void panic_stop_all_processors(void) {
    // We're not ready for this on other arches (or in tests) yet...
#ifdef ARCH_X86_64
    IpwiWorkItem panic = {
            .type = IPWI_TYPE_PANIC_HALT,
            .flags = 0,
    };

    // Lock scheduler on this CPU here, otherwise we might get bumped to another
    // core while we're setting up the IPI...
    const uint64_t lock_flags = sched_lock_this_cpu();
    ipwi_enqueue_all_except_current(&panic);
    ipwi_notify_all_except_current();
    sched_unlock_this_cpu(lock_flags);
#endif
}

void panic_notify_smp_started(void) { smp_is_up = true; }

noreturn void panic_sloc(const char *msg, const char *filename,
                         const uint64_t line) {
    disable_interrupts();
    const uint64_t lock_flags = spinlock_lock_irqsave(&panic_lock);

    if (smp_is_up) {
        panic_stop_all_processors();
    }

    print_header_no_vec(msg);
    print_loc(filename, line);
    print_cpu();
    print_stack_trace();
    print_footer();

    spinlock_unlock_irqrestore(&panic_lock, lock_flags);
    halt_and_catch_fire();
}

noreturn void panic_page_fault_sloc(uintptr_t origin_addr, uintptr_t fault_addr,
                                    uint64_t code, const char *filename,
                                    const uint64_t line) {
    disable_interrupts();
    const uint64_t lock_flags = spinlock_lock_irqsave(&panic_lock);

    if (smp_is_up) {
        panic_stop_all_processors();
    }

    print_header_vec("Page fault", 0x0e);
    print_loc(filename, line);
    print_cpu();
    print_code(code);
    print_page_fault_code(code);
    print_origin_ip(origin_addr);
    print_fault_addr(fault_addr);
    print_stack_trace();
    print_footer();

    spinlock_unlock_irqrestore(&panic_lock, lock_flags);
    halt_and_catch_fire();
}

noreturn void panic_general_protection_fault_sloc(uint64_t code,
                                                  uintptr_t origin_addr,
                                                  const char *filename,
                                                  const uint64_t line) {
    disable_interrupts();
    const uint64_t lock_flags = spinlock_lock_irqsave(&panic_lock);

    if (smp_is_up) {
        panic_stop_all_processors();
    }

    print_header_vec("General protection fault", 0x0d);
    print_loc(filename, line);
    print_cpu();
    print_code(code);
    print_origin_ip(origin_addr);
    print_stack_trace();
    print_footer();

    spinlock_unlock_irqrestore(&panic_lock, lock_flags);
    halt_and_catch_fire();
}

noreturn void panic_exception_with_code_sloc(uint8_t vector, uint64_t code,
                                             uintptr_t origin_addr,
                                             const char *filename,
                                             const uint64_t line) {
    disable_interrupts();
    const uint64_t lock_flags = spinlock_lock_irqsave(&panic_lock);

    if (smp_is_up) {
        panic_stop_all_processors();
    }

    print_header_vec("Unhandled exception", vector);
    print_loc(filename, line);
    print_cpu();
    print_code(code);
    print_origin_ip(origin_addr);
    print_stack_trace();
    print_footer();

    spinlock_unlock_irqrestore(&panic_lock, lock_flags);
    halt_and_catch_fire();
}

noreturn void panic_exception_no_code_sloc(uint8_t vector,
                                           uintptr_t origin_addr,
                                           const char *filename,
                                           const uint64_t line) {
    disable_interrupts();
    const uint64_t lock_flags = spinlock_lock_irqsave(&panic_lock);

    if (smp_is_up) {
        panic_stop_all_processors();
    }

    print_header_vec("Unhandled exception", vector);
    print_loc(filename, line);
    print_cpu();
    print_origin_ip(origin_addr);
    print_stack_trace();
    print_footer();

    spinlock_unlock_irqrestore(&panic_lock, lock_flags);
    halt_and_catch_fire();
}