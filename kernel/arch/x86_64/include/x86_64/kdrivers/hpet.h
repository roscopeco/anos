/*
 * stage3 - HPET kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_DRIVERS_HPET_H
#define __ANOS_KERNEL_ARCH_X86_64_DRIVERS_HPET_H

#include <stdbool.h>
#include <stdint.h>

#include "kdrivers/timer.h"
#include "platform/acpi/acpitables.h"

typedef struct {
    ACPI_SDTHeader header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count : 5;
    uint8_t counter_size : 1;
    uint8_t reserved : 1;
    uint8_t legacy_replacement : 1;
    uint16_t pci_vendor_id;
    ACPI_GenericAddress address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed)) ACPI_HPET;

static_assert_sizeof(ACPI_HPET, ==, 56);

typedef struct {
    uint64_t caps_and_config;
    uint64_t comparator_value;
    uint64_t interrupt_route;
} __attribute__((packed)) HPETTimerRegs;

static_assert_sizeof(HPETTimerRegs, ==, 24);

typedef struct {
    uint64_t caps_and_id;
    uint64_t pad1;

    uint64_t flags;
    uint64_t pad2;

    uint64_t interrupt_status;
    uint64_t pad3;

    uint64_t reserved[24];

    uint64_t counter_value;
    uint64_t pad4;

    HPETTimerRegs timers[];
} __attribute__((packed)) HPETRegs;

static_assert_sizeof(HPETRegs, ==, 256);

static inline ACPI_HPET *acpi_tables_find_hpet(ACPI_RSDT *rsdt) { return (ACPI_HPET *)acpi_tables_find(rsdt, "HPET"); }

static inline uint32_t hpet_period(uint64_t hpet_caps) { return (hpet_caps & 0xffffffff00000000) >> 32; }

static inline uint16_t hpet_vendor(uint64_t hpet_caps) { return (hpet_caps & 0xffff0000) >> 16; }

static inline uint8_t hpet_timer_count(uint64_t hpet_caps) { return ((hpet_caps & 0x1f00) >> 8) + 1; }

static inline bool hpet_is_64_bit(uint64_t hpet_caps) { return ((hpet_caps & 0x2000) != 0); }

static inline bool hpet_can_legacy(uint64_t hpet_caps) { return ((hpet_caps & 0x8000) != 0); }

bool hpet_init(ACPI_RSDT *rsdt);
KernelTimer *hpet_as_timer(void);

#endif //__ANOS_KERNEL_ARCH_X86_64_DRIVERS_HPET_H
