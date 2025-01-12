/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

noreturn void halt_and_catch_fire(void) {
    __asm__ volatile("cli\n\t");

    while (true) {
        __asm__ volatile("hlt\n\t");
    }
}

void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void restore_interrupts(uint64_t flags) {
    __asm__ volatile("pushq %0\n\t" // Push flags onto stack
                     "popfq"        // Pop into RFLAGS
                     :              // No outputs
                     : "r"(flags)   // Input: flags to restore
                     : "memory"     // Memory clobber
    );
}

uint64_t disable_interrupts() {

    uint64_t flags;

    // Read current RFLAGS and disable interrupts atomically
    __asm__ volatile("pushfq\n\t"  // Push RFLAGS onto stack
                     "popq %0\n\t" // Pop into flags variable
                     "cli"         // Clear interrupt flag
                     : "=r"(flags) // Output: flags
                     :             // No inputs
                     : "memory"    // Memory clobber
    );

    return flags;
}
