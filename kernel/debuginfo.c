/*
 * stage3 - Optional startup debug info printing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "acpitables.h"
#include "debugprint.h"
#include "machine.h"
#include "printhex.h"

#ifndef NULL
#define NULL (((void *)0))
#endif

static char *MEM_TYPES[] = {"INVALID",  "AVAILABLE",  "RESERVED",
                            "ACPI",     "NVS",        "UNUSABLE",
                            "DISABLED", "PERSISTENT", "UNKNOWN"};

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

void debug_madt(ACPI_RSDT *rsdt) {
    ACPI_MADT *madt = acpi_tables_find_madt(rsdt);

    if (madt == NULL) {
        debugstr("(ACPI MADT table not found)\n");
        return;
    }

    debugstr("MADT length    : ");
    printhex32(madt->header.length, debugchar);
    debugstr("\n");

    debugstr("LAPIC address  : ");
    printhex32(madt->lapic_address, debugchar);
    debugstr("\n");
    debugstr("Flags          : ");
    printhex32(madt->lapic_address, debugchar);
    debugstr("\n");

    uint16_t remain = madt->header.length - 0x2C;
    uint8_t *ptr = ((uint8_t *)madt) + 0x2C;

    while (remain > 0) {
        uint8_t *type = ptr++;
        uint8_t *len = ptr++;

        uint16_t *flags16;
        uint32_t *flags32;

        switch (*type) {
        case 0: // Processor local APIC
            debugstr("  CPU            [ID: ");
            printhex8(*ptr++, debugchar);
            debugstr("; LAPIC ");
            printhex8(*ptr++, debugchar);
            flags32 = (uint32_t *)ptr;
            debugstr("; Flags: ");
            printhex32(*flags32, debugchar);
            debugstr("]\n");
            ptr += 4;
            break;
        case 1: // IO APIC
            debugstr("  IOAPIC         [ID: ");
            printhex8(*ptr++, debugchar);

            ptr++; // skip reserved

            uint32_t *apicaddr = (uint32_t *)ptr;
            ptr += 4;
            uint32_t *gsibase = (uint32_t *)ptr;
            ptr += 4;

            debugstr("; Addr: ");
            printhex32(*apicaddr, debugchar);
            debugstr("; GSIBase: ");
            printhex32(*gsibase, debugchar);
            debugstr("]\n");
            break;
        case 2: // IP APIC Source Override
            debugstr("  IOAPIC Src O/R [Bus: ");
            printhex8(*ptr++, debugchar);
            debugstr("; IRQ: ");
            printhex8(*ptr++, debugchar);

            uint32_t *gsi = (uint32_t *)ptr;
            ptr += 4;
            debugstr("; GSI: ");
            printhex32(*gsi, debugchar);

            flags16 = (uint16_t *)ptr;
            ptr += 2;
            debugstr("; Flags: ");
            printhex16(*flags16, debugchar);
            debugstr("]\n");

            break;
        case 4: // LAPIC NMI
            debugstr("  LAPIC NMI      [Processor: ");
            printhex8(*ptr++, debugchar);

            flags16 = (uint16_t *)ptr;
            ptr += 2;
            debugstr("; Flags: ");
            printhex16(*flags16, debugchar);

            debugstr("; LINT#: ");
            printhex8(*ptr++, debugchar);
            debugstr("]\n");

            break;
        default:
            // Just skip over
            debugstr("  #TODO UNKNOWN  [Type: ");
            printhex8(*type, debugchar);
            debugstr("; Len: ");
            printhex8(*len, debugchar);
            debugstr("]\n");
            ptr += *len - 2;
        }

        remain -= *len;
    }
}
