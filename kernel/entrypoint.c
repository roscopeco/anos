/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * We're fully 64-bit at this point 🎉
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "printhex.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION         STRVER(VERSTR)

typedef struct {
    uint64_t        base;
    uint64_t        length;
    uint32_t        type;
    uint32_t        attrs;
} __attribute__((packed)) MemMapEntry;

typedef struct {
    uint16_t        num_entries;
    MemMapEntry     entries[];
} __attribute__((packed)) MemMap;

static char *MSG = "ANOS #" VERSION "\n";

static char * MEM_TYPES[] = {
    "INVALID",
    "AVAILABLE",
    "RESERVED",
    "ACPI",
    "NVS",
    "UNUSABLE",
    "DISABLED",
    "PERSISTENT",
    "UNKNOWN"
};

noreturn void halt() {
    while (true) {
        __asm__ volatile (
            "hlt\n\t"
        );
    }
}

void debug_memmap(MemMap *memmap) {
    debugstr("\nThere are ");
    printhex16(memmap->num_entries, debugchar);
    debugstr(" memory map entries\n");

    for (int i = 0; i < memmap->num_entries; i++) {
        MemMapEntry *entry = &memmap->entries[i];

        debugstr("Entry ");
        printhex16(i, debugchar);
        debugstr(": ");
        printhex64(entry->base, debugchar);
        debugstr(" -> ");
        printhex64(entry->base + entry->length, debugchar);

        if (entry->type < 8) {
            debugstr(" (");
            debugstr(MEM_TYPES[entry->type]);
            debugstr(")\n");
        } else {
            debugstr(" (");
            debugstr(MEM_TYPES[8]);
            debugstr(")\n");
        }
    }
}

noreturn void start_kernel(MemMap *memmap) {
    debugstr(MSG);
    debug_memmap(memmap);
    halt();
 }


