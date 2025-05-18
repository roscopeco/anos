/*
 * Machine-specific routines

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include <riscv64/kdrivers/cpu.h>

#ifndef UNIT_TESTS
noreturn
#endif
        void
        halt_and_catch_fire(void) {
    cpu_clear_csr(CSR_SIE, 0);
    cpu_clear_csr(CSR_SIP, 0);

    while (true) {
        __asm__ volatile("wfi");
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
    __asm__ volatile("csrr %0, sstatus\n"
                     "csrc sstatus, %1\n"
                     : "=r"(sstatus)
                     : "r"(0x2) // Clear SIE (Supervisor Interrupt Enable)
                     : "memory");
    return sstatus;
}

void restore_saved_interrupts(uint64_t sstatus) {
    __asm__ volatile("csrw sstatus, %0" : : "r"(sstatus) : "memory");
}