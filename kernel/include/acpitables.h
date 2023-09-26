/*
 * stage3 - ACPI table routines
 * anos - An Operating System
 * 
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_KERNEL_ACPITABLES_H
#define __ANOS_KERNEL_ACPITABLES_H

#include <stdint.h>

#define ACPI_R0_RSDP_SIZE       20

#define ACPI_TABLES_VADDR_BASE  0xFFFFFFFF81000000
#define ACPI_TABLES_VADDR_LIMIT 0xFFFFFFFF81008000

/*
 * Root System Description Pointer (RSDP)
 */
typedef struct {
    char                signature[8];
    uint8_t             checksum;
    char                oem_id[6];
    uint8_t             revision;
    uint32_t            rsdt_address;

    // Following only when revision > 0
    uint32_t            length;
    uint64_t            xsdt_address;
    uint8_t             extended_checksum;
    uint8_t             reserved[3];
} __attribute__((packed)) BIOS_RSDP;


/*
 * System Description Table (SDP) Header
 */
typedef struct {
    char                signature[4];
    uint32_t            length;
    uint8_t             revision;
    uint8_t             checksum;
    char                oem_id[6];
    char                oem_table_id[8];
    uint32_t            oem_revision;
    uint32_t            creator_id;
    uint32_t            creator_revision;
} __attribute__((packed)) BIOS_SDTHeader;

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
 * * Where they are in the same physical page, they'll end up in the same virtual one
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
BIOS_SDTHeader* map_acpi_tables(BIOS_RSDP *rsdp);

BIOS_SDTHeader* find_acpi_table(BIOS_SDTHeader *rsdp, const char *ident);


#endif//__ANOS_KERNEL_ACPITABLES_H
