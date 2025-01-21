/*
 * stage3 - HPET Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>

#include "acpitables.h"
#include "debugprint.h"
#include "kdrivers/hpet.h"
#include "printhex.h"

bool init_hpet(ACPI_RSDT *rsdt) {
    ACPI_HPET *hpet = acpi_tables_find_hpet(rsdt);

    if (hpet) {
#ifdef DEBUG_HPET
        debugstr("Found HPET ");
        printhex8(hpet->hpet_number, debugchar);
        debugstr(" with ");
        printhex8(hpet->comparator_count, debugchar);
        debugstr(" comparators [PCI Vendor ");
        printhex16(hpet->pci_vendor_id, debugchar);
        debugstr("]\n");

        debugstr("  Address: ");
        printhex64(hpet->address.address, debugchar);
        debugstr("\n");

        debugstr("  Counter size: ");
        printhex8(hpet->counter_size, debugchar);
        debugstr("\n");

        debugstr("  Minimum tick: ");
        printhex16(hpet->minimum_tick, debugchar);
        debugstr("\n");

        debugstr("  Page protection: ");
        printhex8(hpet->page_protection, debugchar);
        debugstr("\n");

        debugstr("  HW rev ID: ");
        printhex8(hpet->hardware_rev_id, debugchar);
        debugstr("\n");
#endif

        return true;
    } else {
#ifdef DEBUG_HPET
        debugstr("No HPET...\n");
#endif
        return false;
    }
}
