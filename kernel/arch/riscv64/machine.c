/*
 * Machine-specific routines

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include <riscv64/kdrivers/cpu.h>

void wait_for_interrupt(void) { __asm__ volatile("wfi"); }

#ifndef UNIT_TESTS
noreturn
#endif

        void
        halt_and_catch_fire(void) {
    cpu_clear_sie(0);
    cpu_clear_sip(0);

    while (true) {
        wait_for_interrupt();
    }
}

void disable_interrupts() {
    __asm__ volatile("csrrc zero, sstatus, %0"
                     :
                     : "r"(0x2) // Clear SIE (bit 1)
                     : "memory");
}

void enable_interrupts() {
    __asm__ volatile("csrrs zero, sstatus, %0"
                     :
                     : "r"(0x2) // Set SIE (bit 1)
                     : "memory");
}

uint64_t save_disable_interrupts() {
    uint64_t sstatus;
    __asm__ volatile("csrrci %0, sstatus, 2" : "=r"(sstatus) : : "memory");
    return sstatus;
}

void restore_saved_interrupts(uint64_t sstatus) {
    __asm__ volatile("csrw sstatus, %0" : : "r"(sstatus) : "memory");
}

void kernel_timer_eoe(void) {
    // TODO clear interrupts on RISC-V!
}