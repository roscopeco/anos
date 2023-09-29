/*
 * stage3 - Kernel driver management
 * anos - An Operating System
 * 
 * Copyright (c) 2023 Ross Bamford
 */

#include <stddef.h>

#include "machine.h"
#include "acpitables.h"
#include "debugprint.h"
#include "printhex.h"
#include "vmm/vmmapper.h"
#include "kdrivers/drivers.h"

// TODO Obviously doesn't belong here, just a hack for proof of life...
static void init_local_apics(BIOS_SDTHeader* madt) {
    debugstr("Init local APICs...\n");

    uint32_t *lapic_addr = ((uint32_t*)(madt + 1));
    uint32_t *flags = lapic_addr + 1;
    debugstr("LAPIC address (phys : virt) = ");
    printhex32(*lapic_addr, debugchar);
    debugstr(" : 0xFFFF800000000000 [");
    printhex32(*flags, debugchar);
    debugstr("]\n");

    vmm_map_page(STATIC_PML4, KERNEL_HARDWARE_VADDR_BASE, *lapic_addr, PRESENT | WRITE);

    uint32_t volatile *lapic = (uint32_t*)(KERNEL_HARDWARE_VADDR_BASE);

    debugstr("LAPIC ID: ");
    printhex32(*(lapic + 4), debugchar);
    debugstr("; Version: ");
    printhex32(*(lapic + 8), debugchar);
    debugstr("\n");

    // Set spurious interrupt and enable
    *(lapic + 60) = 0x1FF;

    // Set up timer
    *(lapic + 248) = 0x03;  // /16 mode
    *(lapic + 224) = 20000000;  // 20000000 init count
    *(lapic + 200) = 0x20020;
}

void init_kernel_drivers(BIOS_SDTHeader *rsdt) {
    // Init local APICs
    BIOS_SDTHeader* madt = find_acpi_table(rsdt, "APIC");

    if (madt == NULL) {
        debugstr("No MADT; Halting\n");
        halt_and_catch_fire();
    }

    init_local_apics(madt);
}

void local_apic_eoe() {
    uint32_t volatile *lapic = (uint32_t*)(KERNEL_HARDWARE_VADDR_BASE);
    *(lapic + 44) = 0;
}
