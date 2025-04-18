/*
 * stage3 - Optional startup memmap debug info printing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "machine.h"
#include "printhex.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

static char *const E820H_MEM_TYPES[] = {
        "INVALID",  "AVAILABLE", "RESERVED",   "ACPI",    "NVS",
        "UNUSABLE", "DISABLED",  "PERSISTENT", "UNKNOWN",
};

static char *const LIMINE_MEM_TYPES[] = {
        "USABLE",
        "RESERVED",
        "ACPI_RECLAIMABLE",
        "ACPI_NVS",
        "BAD_MEMORY",
        "BOOTLOADER_RECLAIMABLE",
        "EXECUTABLE_AND_MODULES",
        "FRAMEBUFFER",
};

void debug_memmap_e820(E820h_MemMap *memmap) {
    debugstr("\nThere are ");
    printhex16(memmap->num_entries, debugchar);
    debugstr(" memory map entries\n");

    for (int i = 0; i < memmap->num_entries; i++) {
        E820h_MemMapEntry *entry = &memmap->entries[i];

        debugstr("Entry ");
        printhex16(i, debugchar);
        debugstr(": ");
        printhex64(entry->base, debugchar);
        debugstr(" -> ");
        printhex64(entry->base + entry->length, debugchar);

        if (entry->type < 8) {
            debugstr(" (");
            debugstr(E820H_MEM_TYPES[entry->type]);
            debugstr(")\n");
        } else {
            debugstr(" (");
            debugstr(E820H_MEM_TYPES[8]);
            debugstr(")\n");
        }
    }
}

void debug_memmap_limine(Limine_MemMap *memmap) {
    debugstr("\nThere are ");
    printhex16(memmap->entry_count, debugchar);
    debugstr(" memory map entries\n");

    for (int i = 0; i < memmap->entry_count; i++) {
        Limine_MemMapEntry *entry = memmap->entries[i];

        debugstr("Entry ");
        printhex16(i, debugchar);
        debugstr(": ");
        printhex64(entry->base, debugchar);
        debugstr(" -> ");
        printhex64(entry->base + entry->length, debugchar);

        if (entry->type < 8) {
            debugstr(" (");
            debugstr(LIMINE_MEM_TYPES[entry->type]);
            debugstr(")\n");
        } else {
            debugstr(" (");
            debugstr(LIMINE_MEM_TYPES[8]);
            debugstr(")\n");
        }
    }
}
