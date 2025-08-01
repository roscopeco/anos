/*
 * stage3 - ACPI table routines
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "debugprint.h"
#include "machine.h"
#include "platform/acpi/acpitables.h"
#include "vmm/vmmapper.h"

#ifdef DEBUG_ACPI
#include "printdec.h"
#include "printhex.h"
#endif

typedef struct {
    uint64_t phys;
    uint64_t virt;
} AddressMapping;

// TODO This is wasteful (1KiB) - move it somewhere not in bss...
static AddressMapping page_stack[64];
static uint16_t page_stack_ptr;
static uint64_t next_vaddr = ACPI_TABLES_VADDR_BASE;

static inline uint32_t RSDT_ENTRY_COUNT(ACPI_RSDT *sdt) {
    return ((sdt->header.length - sizeof(ACPI_SDTHeader)) / 4);
}

static inline uint32_t XSDT_ENTRY_COUNT(ACPI_RSDT *sdt) {
    return ((sdt->header.length - sizeof(ACPI_SDTHeader)) / 8);
}

static bool checksum_rsdp(ACPI_RSDP *rsdp) {
#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
    debugstr("### ACPI RSDP CHECKSUM: ");
    printhex64((uintptr_t)rsdp, debugchar);
    debugstr(" [len ");
    printdec(rsdp->length, debugchar);
    debugstr("]\n");
#endif
#endif

    uint8_t *byteptr = (uint8_t *)rsdp;
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

static bool checksum_sdt(ACPI_SDTHeader *sdt) {
#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
    debugstr("### ACPI SDT CHECKSUM: ");
    printhex64((uintptr_t)sdt, debugchar);
    debugstr(" [len ");
    printdec(sdt->length, debugchar);
    debugstr("]\n");
#endif
#endif

    uint8_t *byteptr = (uint8_t *)sdt;
    uint8_t sum = 0;

    for (int i = 0; i < sdt->length; i++) {
        sum += byteptr[i];
    }

    return sum == 0;
}

// Returns 0 if a new mapping is needed but we're at the limit...
static uint64_t get_mapping_for(uint64_t phys) {
    // TODO this is great, until one of the tables crosses a page boundary 🙄

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
    debugstr("Mapping ACPI at ");
    printhex64(phys, debugchar);
#endif
#endif

    if (phys > 0x400000) {
        // not in already-mapped low 4MiB region...
        for (int i = 0; i < page_stack_ptr; i++) {
            if ((phys & PAGE_ALIGN_MASK) == page_stack[i].phys) {
#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
                debugstr(": Using stacked mapping at ");
                printhex64(page_stack[i].virt, debugchar);
                debugstr("\n");
#endif
#endif

                return (phys & PAGE_RELATIVE_MASK) | page_stack[i].virt;
            }
        }

        // not found, map next 32KiB
        if (next_vaddr == (ACPI_TABLES_VADDR_LIMIT)) {
            return 0;
        }

        uint64_t vaddr = next_vaddr;

        /// TODO HACK HACK HACK
        // Mapping eight at a time to side-step the tables-crossing-boundaries issue :D
        //
        // This doesn't _fix_ things though - we really need to map based on the table
        // position and size relative to end of the page as we run through them...
        next_vaddr += 0x8000;
        vmm_map_page_containing(vaddr, phys, PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x1000, phys + 0x1000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x2000, phys + 0x3000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x3000, phys + 0x4000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x4000, phys + 0x5000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x5000, phys + 0x6000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x6000, phys + 0x7000,
                                PG_PRESENT | PG_WRITE);
        vmm_map_page_containing(vaddr + 0x7000, phys + 0x8000,
                                PG_PRESENT | PG_WRITE);

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
        debugstr(": Adding new mapping at ");
        printhex64(vaddr, debugchar);
        debugstr("\n");
#endif
#endif

        // Stack it
        page_stack[page_stack_ptr].virt = vaddr;
        page_stack[page_stack_ptr++].phys = (phys & PAGE_ALIGN_MASK);

        // Fin
        return ((phys & PAGE_RELATIVE_MASK) | vaddr);
    } else {
        // TODO don't keep doing this, relying on this pre-mapped 4MiB is not
        // good...
        //
#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
        debugstr(": Using low-memory mapping at ");
        printhex64((phys | 0xFFFFFFFF80000000), debugchar);
        debugstr("\n");
#endif
#endif

        return (phys | 0xFFFFFFFF80000000);
    }
}

static inline bool has_sig(const char *expect, const ACPI_SDTHeader *sdt) {
    for (int i = 0; i < 4; i++) {
        if (expect[i] != sdt->signature[i]) {
            return false;
        }
    }

    return true;
}

__attribute__((no_sanitize("alignment"))) // ACPI tables can be unaligned...
static ACPI_SDTHeader *
map_sdt(uint64_t phys_addr) {
    uint64_t vaddr = get_mapping_for(phys_addr);

    if (vaddr == 0) {
        // Mapping failed
#ifdef DEBUG_ACPI
        debugstr("Failed to find a virtual address for SDT physical ");
        printhex64(phys_addr, debugchar);
        debugstr("\n");
#endif
        return NULL;
    }

    ACPI_SDTHeader *sdt = (ACPI_SDTHeader *)(vaddr);

    if (!checksum_sdt(sdt)) {
#ifdef DEBUG_ACPI
        debugstr("Checksum failed for SDT physical ");
        printhex64(phys_addr, debugchar);
        debugstr("\n");
#endif
        return NULL;
    }

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
    debugstr("SDT checksum passed; Ident is '");
    debugstr_len(sdt->signature, 4);
    debugstr("'\n");
#endif
#endif

    if (has_sig("RSDT", sdt)) {
        // deal with RSDT
        uint32_t entries = RSDT_ENTRY_COUNT((ACPI_RSDT *)sdt);
        uint32_t *entry = ((uint32_t *)(sdt + 1));

#ifdef DEBUG_ACPI
        debugstr("There are ");
        printhex32(entries, debugchar);
        debugstr(" entries in the ACPI tables\n");
#endif

        for (int i = 0; i < entries; i++) {
            *entry = (uint32_t)(((uint64_t)map_sdt((uint64_t)*entry)) &
                                0xFFFFFFFF);
            entry++;
        }
    } else if (has_sig("XSDT", sdt)) {
        // deal with XSDT
        uint32_t entries = XSDT_ENTRY_COUNT((ACPI_RSDT *)sdt);
        uint64_t *entry = ((uint64_t *)(sdt + 1));

#ifdef DEBUG_ACPI
        debugstr("There are ");
        printhex32(entries, debugchar);
        debugstr(" entries in the ACPI tables\n");
#endif

        for (int i = 0; i < entries; i++) {
            *entry = (uint64_t)map_sdt(*entry);
            entry++;
        }
    }

    return sdt;
}

static ACPI_RSDT *map_acpi_tables(ACPI_RSDP *rsdp) {
    if (!rsdp) {
#ifdef DEBUG_ACPI
        debugstr("Cannot map NULL RSDP!\n");
#endif
        return NULL;
    }

    if (!checksum_rsdp(rsdp)) {
#ifdef DEBUG_ACPI
        debugstr("RSDP checksum failed!\n");
#endif
        return NULL;
    }

    if (rsdp->revision > 1) {
        return (ACPI_RSDT *)map_sdt(rsdp->xsdt_address);
    } else {
        return (ACPI_RSDT *)map_sdt(rsdp->rsdt_address);
    }
}

ACPI_RSDT *acpi_tables_init(ACPI_RSDP *rsdp) { return map_acpi_tables(rsdp); }

__attribute__((no_sanitize("alignment"))) // ACPI tables can be unaligned...
ACPI_SDTHeader *
acpi_tables_find(ACPI_RSDT *rsdt, const char *ident) {
    if (rsdt == NULL || ident == NULL) {
        return NULL;
    }

    if (has_sig("XSDT", &rsdt->header)) {
        uint32_t entries = XSDT_ENTRY_COUNT(rsdt);
        uint64_t *entry = ((uint64_t *)(rsdt + 1));

        for (int i = 0; i < entries; i++) {
#ifdef UNIT_TESTS
            ACPI_SDTHeader *sdt = (ACPI_SDTHeader *)(((uint64_t)*entry));
#else
            ACPI_SDTHeader *sdt =
                    (ACPI_SDTHeader *)(*entry | 0xFFFFFFFF00000000);
#endif

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
            debugstr("Find ACPI entry: Checking: ");
            printhex64(((uint64_t)entry) | 0xFFFFFFFF00000000, debugchar);
            debugstr(" = ");
            debugstr_len(sdt->signature, 4);
            debugstr("\n");
#endif
#endif

            if (has_sig(ident, sdt)) {
                return sdt;
            }
            entry++;
        }
    } else if (has_sig("RSDT", &rsdt->header)) {
        uint32_t entries = RSDT_ENTRY_COUNT(rsdt);
        uint32_t *entry = ((uint32_t *)(rsdt + 1));

        for (int i = 0; i < entries; i++) {
#ifdef UNIT_TESTS
            ACPI_SDTHeader *sdt = (ACPI_SDTHeader *)(((uint64_t)*entry));
#else
            ACPI_SDTHeader *sdt =
                    (ACPI_SDTHeader *)(((uint64_t)*entry) | 0xFFFFFFFF00000000);
#endif

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
            debugstr("Find ACPI entry: Checking: ");
            printhex64(((uint64_t)entry) | 0xFFFFFFFF00000000, debugchar);
            debugstr(" = ");
            debugstr_len(sdt->signature, 4);
            debugstr("\n");
#endif
#endif

            if (has_sig(ident, sdt)) {
                return sdt;
            }
            entry++;
        }
    } else {
#ifdef CONSERVATIVE_BUILD
        debugstr(
                "CONSERVATIVE: Non-RSDT passed to acpi_tables_find; Halting\n");
        halt_and_catch_fire();
#else
        debugstr("WARNING: Non-RSDT passed to acpi_tables_find!\n");
#endif
    }

    return NULL;
}