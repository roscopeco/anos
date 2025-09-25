/*
 * stage3 - x86_64 specific IPWI handling
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "kdrivers/drivers.h"
#include "x86_64/kdrivers/local_apic.h"

void arch_ipwi_notify_all_except_current(void) {
    // TODO using ALL_EXCLUDING_SELF is probably a bad idea here,
    //      what if we didn't spin up all APs properly?
    //
    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_LAPIC);

    while (*(REG_LAPIC_ICR_LOW(lapic)) & LAPIC_ICR_DELIVERY_STATUS)
        ;

    *(REG_LAPIC_ICR_HIGH(lapic)) = 0;
    *(REG_LAPIC_ICR_LOW(lapic)) =
            LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_MODE_NMI | LAPIC_ICR_DEST_ALL_EXCLUDING_SELF;
}
