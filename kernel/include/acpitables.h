/*
 * stage3 - ACPI table routines
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_ACPITABLES_H
#define __ANOS_KERNEL_ACPITABLES_H

#include <stdint.h>

#include "anos_assert.h"

#define ACPI_R0_RSDP_SIZE 20

#define ACPI_TABLES_VADDR_BASE 0xFFFFFFFF81000000
#define ACPI_TABLES_VADDR_LIMIT 0xFFFFFFFF81008000

/*
 * Root System Description Pointer (RSDP)
 */
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;

    // Following only when revision > 0
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) ACPI_RSDP;

/*
 * System Description Table (SDT) Header
 */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) ACPI_SDTHeader;

typedef struct {
    uint8_t address_space_id; // 0 = Sys Mem, 1 = Sys IO
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} __attribute__((packed)) ACPI_GenericAddress;

typedef struct {
    ACPI_SDTHeader header;
} __attribute__((packed)) ACPI_RSDT;

typedef struct {
    ACPI_SDTHeader header;
    uint32_t lapic_address;
    uint32_t lapic_flags;
} __attribute__((packed)) ACPI_MADT;

static_assert_sizeof(ACPI_RSDP, ==, 36);
static_assert_sizeof(ACPI_SDTHeader, ==, 36);
static_assert_sizeof(ACPI_GenericAddress, ==, 12);
static_assert_sizeof(ACPI_RSDT, ==, 36);
static_assert_sizeof(ACPI_MADT, ==, 44);

/*
 * Validate the ACPI tables and map them into virtual memory.
 *
 * The tables will be mapped into the reserved area of virtual
 * address space based at ACPI_TABLES_VADDR_BASE.
 *
 * Currently, the mapping may be relatively sparse, depending
 * how the tables are laid out by the BIOS:
 *
 * * Nothing is copied or moved in physical RAM
 * * The physical addresses are simply mapped into virtual space
 * * Where they are in the same physical page, they'll end up in the same
 * virtual one
 * * But where they are spread around, they'll end up in different virtual pages
 *
 * Caveats:
 *
 * * If a table crosses a page boundary, this will break!
 *   * I don't know if this is a thing (but I bet it can be...)
 * * If more than eight pages need to be mapped, this will error
 * * If this errors, any pages it did map won't be unmapped
 *   * But it's probably a panic at that point anyway, so 🤷
 *
 * Returns the virtual pointer to the RSDT header. All pointers
 * in this will have been fixed up to their virtual addresses.
 *
 * Pointers in _some_ of the other SDTs will also be fixed - but
 * only the ones I'm actually using right now. More will be added
 * as I need them 🙃
 */
ACPI_RSDT *acpi_tables_init(ACPI_RSDP *rsdp);

ACPI_SDTHeader *acpi_tables_find(ACPI_RSDT *rsdt, const char *ident);

static inline ACPI_MADT *acpi_tables_find_madt(ACPI_RSDT *rsdt) {
    return (ACPI_MADT *)acpi_tables_find(rsdt, "APIC");
}

#endif //__ANOS_KERNEL_ACPITABLES_H
