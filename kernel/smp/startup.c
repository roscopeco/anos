#include <stdint.h>

#include "debugprint.h"
#include "kdrivers/cpu.h"
#include "kdrivers/local_apic.h"

extern void *_binary_kernel_realmode_bin_start,
        *_binary_kernel_realmode_bin_end;

// If you're changing any of these, you'll need to change the real-mode
// link script as well...
//
#define AP_TRAMPOLINE_RUN_PADDR ((0x4000))

#define AP_TRAMPOLINE_BASE_VADDR                                               \
    (((void *)(0xffffffff80000000 | AP_TRAMPOLINE_RUN_PADDR)))
#define AP_TRAMPOLINE_BIN_START (((void *)&_binary_kernel_realmode_bin_start))
#define AP_TRAMPOLINE_BIN_LENGTH                                               \
    (((&_binary_kernel_realmode_bin_end - &_binary_kernel_realmode_bin_start)))

static inline void memcpy(volatile void *dest, volatile void *src,
                          uint64_t count) {
    uint8_t volatile *s = (uint8_t *)src;
    uint8_t volatile *d = (uint8_t *)dest;

    for (int i = 0; i < count; i++) {
        *d++ = *s++;
    }
}

/*
 * Must only be called by the BSP for now!
 */
void smp_bsp_start_aps(uint32_t volatile *lapic) {
    // nothing yet
}

void smp_bsp_start_ap(uint8_t ap_id, uint32_t volatile *lapic) {
    // copy the AP trampoline code to a fixed address in low conventional memory (to address 0x0400:0x0000)
    memcpy(AP_TRAMPOLINE_BASE_VADDR, AP_TRAMPOLINE_BIN_START,
           AP_TRAMPOLINE_BIN_LENGTH);

    // Send INIT
    *(REG_LAPIC_ICR_HIGH(lapic)) = ap_id << 24;
    *(REG_LAPIC_ICR_LOW(lapic)) = 0x4500;

    cpu_tsc_mdelay(10);

    // Send SIPI
    *(REG_LAPIC_ICR_HIGH(lapic)) = ap_id << 24;
    *(REG_LAPIC_ICR_LOW(lapic)) = 0x4600 | (AP_TRAMPOLINE_RUN_PADDR >> 12);

    uint8_t volatile *b = (uint8_t *)0xffffffff80004010;
    *b = 0;
    while (!*b) {
    }

    debugstr("Second CPU is up...\n");
}