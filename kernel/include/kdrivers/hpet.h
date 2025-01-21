/*
 * stage3 - HPET kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DRIVERS_HPET_H
#define __ANOS_KERNEL_DRIVERS_HPET_H

#include <stdbool.h>
#include <stdint.h>

#include "acpitables.h"

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

static inline ACPI_HPET *acpi_tables_find_hpet(ACPI_RSDT *rsdt) {
    return (ACPI_HPET *)acpi_tables_find(rsdt, "HPET");
}

bool init_hpet(ACPI_RSDT *rsdt);

#endif //__ANOS_KERNEL_DRIVERS_HPET_H
