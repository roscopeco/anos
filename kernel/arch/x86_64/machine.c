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

#include <x86_64/kdrivers/local_apic.h>

void wait_for_interrupt(void) { __asm__ volatile("hlt"); }

#ifndef UNIT_TESTS
noreturn
#endif
        void halt_and_catch_fire(void) {
    __asm__ volatile("cli\n\t");

    while (true) {
        wait_for_interrupt();
    }
}

void outl(uint16_t port, uint32_t value) { __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port) : "memory"); }

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

void outb(uint16_t port, uint8_t value) { __asm__ volatile("outb %b0, %w1" : : "a"(value), "Nd"(port) : "memory"); }

uint8_t inb(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inb %w1, %b0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

void disable_interrupts() { __asm__ volatile("cli\n"); }

void enable_interrupts() { __asm__ volatile("sti\n"); }

uint64_t save_disable_interrupts() {

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

void restore_saved_interrupts(uint64_t flags) {
    __asm__ volatile("pushq %0\n\t" // Push flags onto stack
                     "popfq"        // Pop into RFLAGS
                     :              // No outputs
                     : "r"(flags)   // Input: flags to restore
                     : "memory"     // Memory clobber
    );
}

void kernel_timer_eoe(void) { local_apic_eoe(); }