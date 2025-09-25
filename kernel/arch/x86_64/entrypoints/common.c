/*
 * stage3 - Common code used by various entry points
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include "x86_64/gdt.h"
#include "x86_64/interrupts.h"
#include "x86_64/kdrivers/cpu.h"

// Replace the bootstrap 32-bit segments with 64-bit user segments.
//
// TODO we should remap the memory as read-only after this since they
// won't be changing again, accessed bit is already set ready for this...
//
void init_kernel_gdt(void) {
    GDTR gdtr;

    cpu_store_gdtr(&gdtr);

    // Reverse ordered because SYSRET is fucking weird...
    GDTEntry *user_data = get_gdt_entry(&gdtr, 3);
    GDTEntry *user_code = get_gdt_entry(&gdtr, 4);

    init_gdt_entry(user_code, 0, 0,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(3) | GDT_ENTRY_ACCESS_NON_SYSTEM |
                           GDT_ENTRY_ACCESS_EXECUTABLE | GDT_ENTRY_ACCESS_READ_WRITE | GDT_ENTRY_ACCESS_ACCESSED,
                   GDT_ENTRY_FLAGS_64BIT);

    init_gdt_entry(user_data, 0, 0,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(3) | GDT_ENTRY_ACCESS_NON_SYSTEM |
                           GDT_ENTRY_ACCESS_READ_WRITE | GDT_ENTRY_ACCESS_ACCESSED,
                   GDT_ENTRY_FLAGS_64BIT);
}

void install_interrupts() { idt_install(0x08); }
