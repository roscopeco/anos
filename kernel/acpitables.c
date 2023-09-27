/*
 * stage3 - ACPI table routines
 * anos - An Operating System
 * 
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "machine.h"
#include "debugprint.h"
#include "printhex.h"
#include "acpitables.h"
#include "vmm/vmmapper.h"

#define RSDT_ENTRY_COUNT(sdt)   ((sdt->length - sizeof(BIOS_SDTHeader)) / 4) // TODO hard-coded to 32-bit rev0

typedef struct {
    uint64_t        phys;
    uint64_t        virt;
} AddressMapping;

// TODO This is wasteful (1KiB) - move it somewhere not in bss...
static AddressMapping page_stack[64];
static uint16_t page_stack_ptr;
static uint64_t next_vaddr = ACPI_TABLES_VADDR_BASE;

static bool checksum_rsdp(BIOS_RSDP *rsdp) {
    uint8_t *byteptr = (uint8_t*)rsdp;
    uint8_t sum = 0;

    uint32_t len;
    if (rsdp->revision == 0) {
        len = ACPI_R0_RSDP_SIZE;
    } else {
        len = rsdp->length;
    }

    for (int i = 0; i < len; i++) {
        sum += byteptr[i];
    }

    return sum == 0;
}

static bool checksum_sdt(BIOS_SDTHeader *sdt) {
    uint8_t *byteptr = (uint8_t*)sdt;
    uint8_t sum = 0;

    for (int i = 0; i < sdt->length; i++) {
        sum += byteptr[i];
    }

    return sum == 0;
}

// Returns 0 if a new mapping is needed but we're at the limit...
static uint64_t get_mapping_for(uint64_t phys) {
    // TODO this is great, until one of the tables crosses a page boundary 🙄

#   ifdef DEBUG_ACPI
#   ifdef VERY_NOISY_ACPI
    debugstr("Mapping ACPI at ");
    printhex64(phys, debugchar);
#   endif
#   endif

    if (phys > 0x400000) {
        // not in already-mapped low 4MiB region...
        for (int i = 0; i < page_stack_ptr; i++) {
            if ((phys & PAGE_ALIGN_MASK) == page_stack[i].phys) {
#               ifdef DEBUG_ACPI
#               ifdef VERY_NOISY_ACPI
                debugstr(": Using stacked mapping at ");
                printhex64(page_stack[i].virt, debugchar);
                debugstr("\n");
#               endif
#               endif

                return (phys & PAGE_RELATIVE_MASK) | page_stack[i].virt;
            }
        }

        // not found, map new
        if (next_vaddr == ACPI_TABLES_VADDR_LIMIT) {
            return 0;
        }

        uint64_t vaddr = next_vaddr;
        next_vaddr += 0x1000;        
        map_page_containing(STATIC_PML4, vaddr, phys, PRESENT);

#       ifdef DEBUG_ACPI
#       ifdef VERY_NOISY_ACPI
        debugstr(": Adding new mapping at ");
        printhex64(vaddr, debugchar);
        debugstr("\n");
#       endif
#       endif

        // Stack it
        page_stack[page_stack_ptr].virt = vaddr;
        page_stack[page_stack_ptr++].phys = (phys & PAGE_ALIGN_MASK);

        // Fin
        return ((phys & PAGE_RELATIVE_MASK) | vaddr);
    } else {
        // TODO don't keep doing this, relying on this pre-mapped 4MiB is not good...
        // 
#       ifdef DEBUG_ACPI
#       ifdef VERY_NOISY_ACPI
        debugstr(": Using low-memory mapping at ");
        printhex64((phys | 0xFFFFFFFF80000000), debugchar);
        debugstr("\n");
#       endif
#       endif

        return (phys | 0xFFFFFFFF80000000);
    }
}

bool has_sig(const char *expect, BIOS_SDTHeader *sdt) {
    for (int i = 0; i < 4; i++) {
        if (expect[i] != sdt->signature[i]) {
            return false;
        }
    }

    return true;
}

static BIOS_SDTHeader* map_sdt(uint64_t phys_addr) {
    uint64_t vaddr = get_mapping_for(phys_addr);

    if (vaddr == 0) {
        // Mapping failed
#       ifdef DEBUG_ACPI
        debugstr("Failed to find a virtual address for SDT physical ");
        printhex64(phys_addr, debugchar);
        debugstr("\n");
#       endif
        return NULL;
    }
    
    BIOS_SDTHeader *sdt = (BIOS_SDTHeader*)(vaddr);    

    if (!checksum_sdt(sdt)) {
#       ifdef DEBUG_ACPI
        debugstr("Checksum failed for SDT physical ");
        printhex64(phys_addr, debugchar);
        debugstr("\n");
#       endif
        return NULL;
    }

#   ifdef DEBUG_ACPI
#   ifdef VERY_NOISY_ACPI
    debugstr("SDT checksum passed; Ident is '");
    debugstr_len(sdt->signature, 4);
    debugstr("'\n");
#   endif
#   endif

    if (has_sig("RSDT", sdt)) {
        // deal with RSDT
        uint32_t entries = RSDT_ENTRY_COUNT(sdt);
        uint32_t *entry = ((uint32_t*)(sdt + 1));

#       ifdef DEBUG_ACPI
        debugstr("There are ");
        printhex32(entries, debugchar);
        debugstr(" entries in the ACPI tables\n");
#       endif

        for (int i = 0; i < entries; i++) {
            *entry = (uint32_t)(((uint64_t)map_sdt((uint64_t)*entry)) & 0xFFFFFFFF);
            entry++;
        }
    }

    return sdt;
}

BIOS_SDTHeader* map_acpi_tables(BIOS_RSDP *rsdp) {
    if (!rsdp) {
#       ifdef DEBUG_ACPI
        debugstr("Cannot map NULL RSDP!\n");
#       endif
        return NULL;
    }

    if (!checksum_rsdp(rsdp)) {
#       ifdef DEBUG_ACPI
        debugstr("RSDP checksum failed!\n");
#       endif
        return NULL;
    }

    return map_sdt(rsdp->rsdt_address);
}

BIOS_SDTHeader* find_acpi_table(BIOS_SDTHeader *rsdp, const char *ident) {
    uint32_t entries = RSDT_ENTRY_COUNT(rsdp);
    uint32_t *entry = ((uint32_t*)(rsdp + 1));

    for (int i = 0; i < entries; i++) {    
        BIOS_SDTHeader* sdt = (BIOS_SDTHeader*)(((uint64_t)*entry) | 0xFFFFFFFF00000000);

#       ifdef DEBUG_ACPI
#       ifdef VERY_NOISY_ACPI
        debugstr("Find ACPI entry: Checking: ");
        printhex64(((uint64_t)entry) | 0xFFFFFFFF00000000, debugchar);
        debugstr(" = ");
        debugstr_len(sdt->signature, 4);
        debugstr("\n");
#       endif
#       endif

        if (has_sig(ident, sdt)) {
            return sdt;
        }
        entry++;
    }

    return NULL;
}