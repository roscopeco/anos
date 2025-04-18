/*
 * Machine-specific routines

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>

#include <kdrivers/cpu.h>

noreturn void halt_and_catch_fire(void) {
    cpu_clear_csr(CSR_SIE, 0);
    cpu_clear_csr(CSR_SIP, 0);

    while (true) {
        __asm__ volatile("wfi");
    }
}
