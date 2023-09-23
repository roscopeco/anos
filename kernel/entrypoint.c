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
#include "init_pagetables.h"
#include "pmm/pagealloc.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

// This is the static virtual address region (128GB from this base)
// that is reserved for PMM structures and stack.
#ifndef STATIC_PMM_VREGION
#define STATIC_PMM_VREGION  ((void*)0xFFFFFF8000000000)
#endif

// The base address of the physical region this allocator should manage.
#ifndef PMM_PHYS_BASE
#define PMM_PHYS_BASE       0x200000
#endif

#ifndef VRAM_VIRT_BASE
#define VRAM_VIRT_BASE      ((char * const)0xffffffff800b8000)
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION         STRVER(VERSTR)

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

void debug_memmap(E820h_MemMap *memmap) {
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
            debugstr(MEM_TYPES[entry->type]);
            debugstr(")\n");
        } else {
            debugstr(" (");
            debugstr(MEM_TYPES[8]);
            debugstr(")\n");
        }
    }
}

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

MemoryRegion *physical_region;

noreturn void start_kernel(E820h_MemMap *memmap) {
    debugterm_init(VRAM_VIRT_BASE);    
    banner();

    pagetables_init();
    physical_region = page_alloc_init(memmap, PMM_PHYS_BASE, STATIC_PMM_VREGION);
    install_interrupts();
    
    debugstr("We have ");
    printhex64(physical_region->size, debugchar);
    debugstr(" bytes physical memory\n");

    debug_memmap(memmap);

    debugstr("\nForcing a (handled) page fault with write to 0xFFFFFFFF80600000...\n");

    // Force a page fault for testing purpose. This one should get handled and a page mapped...
    uint32_t *bad = (uint32_t*)0xFFFFFFFF80600000;
    *bad = 0x0A11C001;
    debugstr("Continued after fault, write succeeded. Value is ");
    debugattr(0x02);
    printhex32(*bad, debugchar);
    debugattr(0x07);
    debugstr("\n");

    // Force another page fault, to a non-handled address. Should have the WRITE bit set in the error code...
    debugstr("Forcing another, this time unhandled, with write to 0x1200000\n");
    bad = (uint32_t*)0x1200000;
    *bad = 0x0BADF00D;

    debugstr("All is well! Halting for now.\n");
    halt_and_catch_fire();
 }


