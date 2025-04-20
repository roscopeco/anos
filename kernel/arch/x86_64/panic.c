/*
 * stage3 - x86_64 specific panic handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "kdrivers/drivers.h"
#include "x86_64/kdrivers/local_apic.h"

void arch_panic_stop_all_processors(void) {
    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);

    while (*(REG_LAPIC_ICR_LOW(lapic)) & LAPIC_ICR_DELIVERY_STATUS)
        ;

    *(REG_LAPIC_ICR_HIGH(lapic)) = 0;
    *(REG_LAPIC_ICR_LOW(lapic)) = LAPIC_ICR_LEVEL_ASSERT |
                                  LAPIC_ICR_DELIVERY_MODE_NMI |
                                  LAPIC_ICR_DEST_ALL_EXCLUDING_SELF;
}
