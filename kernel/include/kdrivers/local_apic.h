/*
 * stage3 - Local APIC kernel driver
 * anos - An Operating System
 * 
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DRIVERS_LOCAL_APIC_H
#define __ANOS_KERNEL_DRIVERS_LOCAL_APIC_H

#include <stdint.h>

#define REG_LAPIC_ID_O              0x04
#define REG_LAPIC_VERSION_O         0x08
#define REG_LAPIC_SPURIOUS_O        0x3c
#define REG_LAPIC_DIVIDE_O          0xf8
#define REG_LAPIC_INITIAL_COUNT_O   0xe0
#define REG_LAPIC_LVT_TIMER_O       0xc8

#define LAPIC_REG(lapic, reg)       ((lapic + REG_LAPIC##_##reg##_##O))

#define LAPIC_ID(lapic)             (LAPIC_REG(lapic, ID))
#define LAPIC_VERSION(lapic)        (LAPIC_REG(lapic, VERSION))
#define LAPIC_SPURIOUS(lapic)       (LAPIC_REG(lapic, SPURIOUS))
#define LAPIC_DIVIDE(lapic)         (LAPIC_REG(lapic, DIVIDE))
#define LAPIC_INITIAL_COUNT(lapic)  (LAPIC_REG(lapic, INITIAL_COUNT))
#define LAPIC_LVT_TIMER(lapic)      (LAPIC_REG(lapic, LVT_TIMER))

typedef struct {
    uint64_t        base_address;
    uint8_t         processor_id;
    uint8_t         apic_id;
    uint32_t        flags;
    uint16_t        reserved;
} LocalAPIC;

void init_local_apic(BIOS_SDTHeader* madt);

void local_apic_eoe();

#endif//__ANOS_KERNEL_DRIVERS_LOCAL_APIC_H
