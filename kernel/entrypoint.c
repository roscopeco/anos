/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * We're fully 64-bit at this point 🎉
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include "debugprint.h"
#include "printhex.h"
#include "machine.h"
#include "interrupts.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
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

static char *MSG = VERSION "\n";

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

extern void interrupt_handler_nc(void);
extern void interrupt_handler_wc(void);

static inline void banner() {
    debugattr(0x0B);
    debugchar('A');
    debugchar('N');
    debugchar('O');
    debugchar('S');
    debugattr(0x08);
    debugstr(" #");
    debugattr(0x0F);
    debugstr(MSG);
    debugattr(0x07);
}

static inline void install_interrupts() {
    idt_install(0x18);
}

noreturn void start_kernel(MemMap *memmap) {
    banner();
    install_interrupts();
    debug_memmap(memmap);

    debugstr("\nForcing a page fault...\n");
    // Force a page fault for testing purporse. Should have the WRITE bit set in the error code...
    uint32_t *bad = (uint32_t*)0x200000;
    *bad = 0x0BADF00D;

    debugstr("All is well! Halting for now.");
    halt_and_catch_fire();
 }


