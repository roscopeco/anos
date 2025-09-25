/*
 * stage3 - Local APIC kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_DRIVERS_LOCAL_APIC_H
#define __ANOS_KERNEL_ARCH_X86_64_DRIVERS_LOCAL_APIC_H

#include <stdbool.h>
#include <stdint.h>

#include "platform/acpi/acpitables.h"

// This doesn't belong here, it'll go away when we go tickless...
#define KERNEL_HZ ((100))

#define REG_LAPIC_ID_O 0x08
#define REG_LAPIC_VERSION_O 0x0c
#define REG_LAPIC_EOI_O 0x2c
#define REG_LAPIC_SPURIOUS_O 0x3c
#define REG_LAPIC_DIVIDE_O 0xf8
#define REG_LAPIC_INITIAL_COUNT_O 0xe0
#define REG_LAPIC_CURRENT_COUNT_O 0xe4
#define REG_LAPIC_ICR_LOW_O 0xc0
#define REG_LAPIC_ICR_HIGH_O 0xc4
#define REG_LAPIC_LVT_TIMER_O 0xc8

#define LAPIC_REG(lapic, reg) ((lapic + REG_LAPIC##_##reg##_##O))

#define REG_LAPIC_ID(lapic) (LAPIC_REG(lapic, ID))
#define REG_LAPIC_VERSION(lapic) (LAPIC_REG(lapic, VERSION))
#define REG_LAPIC_EOI(lapic) (LAPIC_REG(lapic, EOI))
#define REG_LAPIC_SPURIOUS(lapic) (LAPIC_REG(lapic, SPURIOUS))
#define REG_LAPIC_DIVIDE(lapic) (LAPIC_REG(lapic, DIVIDE))
#define REG_LAPIC_INITIAL_COUNT(lapic) (LAPIC_REG(lapic, INITIAL_COUNT))
#define REG_LAPIC_CURRENT_COUNT(lapic) (LAPIC_REG(lapic, CURRENT_COUNT))
#define REG_LAPIC_ICR_LOW(lapic) (LAPIC_REG(lapic, ICR_LOW))
#define REG_LAPIC_ICR_HIGH(lapic) (LAPIC_REG(lapic, ICR_HIGH))
#define REG_LAPIC_LVT_TIMER(lapic) (LAPIC_REG(lapic, LVT_TIMER))

#define LAPIC_TIMER_BSP_VECTOR (((uint8_t)0x30))
#define LAPIC_TIMER_AP_VECTOR (((uint8_t)0x31))

// Status bits
#define LAPIC_ICR_DELIVERY_STATUS ((1 << 12))         // Delivery status bit
#define LAPIC_ICR_LEVEL_ASSERT ((1 << 14))            // Level assert
#define LAPIC_ICR_DEST_ALL_EXCLUDING_SELF ((3 << 18)) // Destination shorthand for all except self
#define LAPIC_ICR_DELIVERY_MODE_NMI ((4 << 8))        // Delivery mode for NMI

typedef struct {
    uint64_t base_address;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
    uint16_t reserved;
} LocalAPIC;

uint32_t volatile *init_local_apic(ACPI_MADT *madt, bool bsp);

uint64_t local_apic_get_count(void);

void local_apic_eoe(void);

#endif //__ANOS_KERNEL_ARCH_X86_64_DRIVERS_LOCAL_APIC_H
