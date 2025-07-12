/*
 * The device manager
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anos/syscalls.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

// ACPI Table Structures (matching kernel definitions)
typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) ACPI_RSDP;

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
    ACPI_SDTHeader header;
} __attribute__((packed)) ACPI_RSDT;

// ACPI constants
#define USER_ACPI_BASE 0x8040000000 // Base for mapping ACPI tables

static void print_signature(const char *sig, const size_t len) {
    for (size_t i = 0; i < len && sig[i] != '\0'; i++) {
        printf("%c", sig[i]);
    }
}

static void parse_acpi_rsdp(ACPI_RSDP *rsdp) {
    printf("Parsing ACPI RSDP at 0x%lx...\n", (uintptr_t)rsdp);

    if (!rsdp) {
        printf("No RSDP provided.\n");
        return;
    }

    printf("\nRSDP Information:\n");
    printf("  Signature: ");
    print_signature(rsdp->signature, 8);
    printf("\n  OEM ID: ");
    print_signature(rsdp->oem_id, 6);
    printf("\n  Revision: %u\n", rsdp->revision);
    printf("  RSDT Address: 0x%08x\n", rsdp->rsdt_address);
    printf("  XSDT Address: 0x%016lx\n", rsdp->xsdt_address);

    uint64_t table_addr = 0;
    bool use_xsdt = false;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        table_addr = rsdp->xsdt_address;
        use_xsdt = true;
        printf("\nWill use XSDT at physical address 0x%016lx\n", table_addr);
    } else if (rsdp->rsdt_address != 0) {
        table_addr = rsdp->rsdt_address;
        use_xsdt = false;
        printf("\nWill use RSDT at physical address 0x%08x\n",
               (uint32_t)table_addr);
    } else {
        printf("No valid RSDT or XSDT address found in RSDP.\n");
        return;
    }

    // Map the table (align address down to page boundary and map one page)
    const uint64_t table_phys_page = table_addr & ~0xFFF;
    const uint64_t table_offset = table_addr & 0xFFF;

    printf("Mapping physical page 0x%016lx to user address 0x%lx\n",
           table_phys_page, USER_ACPI_BASE);

    // Map the physical page containing the table
    const SyscallResult result =
            anos_map_physical(table_phys_page, (void *)USER_ACPI_BASE, 4096);
    if (result != SYSCALL_OK) {
        printf("Failed to map ACPI table! Error code: %d\n", result);
        return;
    }

    // Access the table at the correct offset within the mapped page
    ACPI_SDTHeader *table =
            (ACPI_SDTHeader *)((uint8_t *)USER_ACPI_BASE + table_offset);

    if (use_xsdt) {
        printf("\nXSDP (64-bit system descriptor table) at 0x%lx:\n",
               (uintptr_t)table);
        printf("  Signature: ");
        print_signature(table->signature, 4);
        printf("\n  Length: %u bytes\n", table->length);
        printf("  Revision: %u\n", table->revision);
        printf("  OEM ID: ");
        print_signature(table->oem_id, 6);
        printf("\n  OEM Table ID: ");
        print_signature(table->oem_table_id, 8);
        printf("\n  OEM Revision: 0x%x\n", table->oem_revision);

        // Count and display entries (64-bit pointers)
        const uint32_t entry_count =
                (table->length - sizeof(ACPI_SDTHeader)) / 8;
        printf("  Number of entries: %u\n", entry_count);

        const uint64_t *entries = (uint64_t *)(table + 1);
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%016lx\n", i, entries[i]);
        }

        // Let's dump the first table as a test
        if (entry_count > 0) {
            printf("\n--- Dumping first ACPI table ---\n");
            const uint64_t first_table_addr = entries[0];

            printf("Looking for first table at physical 0x%016lx\n",
                   first_table_addr);

            // Map the first table using map_physical
            const uint64_t first_table_phys_page = first_table_addr & ~0xFFF;
            const uint64_t first_table_offset = first_table_addr & 0xFFF;

            printf("Mapping first table at physical 0x%016lx (page 0x%016lx, "
                   "offset 0x%lx)\n",
                   first_table_addr, first_table_phys_page, first_table_offset);

            // Map the first table (use a different base address)
            constexpr uintptr_t first_table_base = USER_ACPI_BASE + 0x1000;
            const SyscallResult table_result = anos_map_physical(
                    first_table_phys_page, (void *)first_table_base, 4096);
            if (table_result != SYSCALL_OK) {
                printf("Failed to map first table! Error code: %d\n",
                       table_result);
            } else {
                ACPI_SDTHeader *first_table =
                        (ACPI_SDTHeader *)((uint8_t *)first_table_base +
                                           first_table_offset);

                printf("First table header:\n");
                printf("  Signature: ");
                print_signature(first_table->signature, 4);
                printf("\n  Length: %u bytes\n", first_table->length);
                printf("  Revision: %u\n", first_table->revision);
                printf("  OEM ID: ");
                print_signature(first_table->oem_id, 6);
                printf("\n  OEM Table ID: ");
                print_signature(first_table->oem_table_id, 8);
                printf("\n  OEM Revision: 0x%x\n", first_table->oem_revision);
                printf("  Creator ID: 0x%08x\n", first_table->creator_id);
                printf("  Creator Revision: 0x%08x\n",
                       first_table->creator_revision);

                // Dump first 64 bytes of table data (after header)
                const uint8_t *table_data = (uint8_t *)(first_table + 1);
                const uint32_t data_size =
                        first_table->length - sizeof(ACPI_SDTHeader);
                const uint32_t dump_size = data_size < 64 ? data_size : 64;

                printf("\nFirst %u bytes of table data:\n", dump_size);
                for (uint32_t i = 0; i < dump_size; i++) {
                    if (i % 16 == 0)
                        printf("  %04x: ", i);
                    printf("%02x ", table_data[i]);
                    if (i % 16 == 15)
                        printf("\n");
                }
                if (dump_size % 16 != 0)
                    printf("\n");
            }
        } else {
            printf("Could not map first ACPI table\n");
        }

        // Dump all table signatures
        printf("\n--- All ACPI Table Signatures ---\n");
        for (uint32_t i = 0; i < entry_count; i++) {
            uint64_t table_addr = entries[i];

            // Map each table to read its signature
            uint64_t table_phys_page = table_addr & ~0xFFF;
            uint64_t table_offset = table_addr & 0xFFF;

            const uintptr_t sig_table_base =
                    USER_ACPI_BASE + 0x2000 + (i * 0x1000);
            const SyscallResult sig_result = anos_map_physical(
                    table_phys_page, (void *)sig_table_base, 4096);
            if (sig_result == SYSCALL_OK) {
                ACPI_SDTHeader *table =
                        (ACPI_SDTHeader *)((uint8_t *)sig_table_base +
                                           table_offset);
                printf("  Table %u: ", i);
                print_signature(table->signature, 4);
                printf("\n");
            } else {
                printf("  Table %u: <mapping failed>\n", i);
            }
        }
    } else {
        printf("\nRSDT (32-bit system descriptor table) at 0x%lx:\n",
               (uintptr_t)table);
        printf("  Signature: ");
        print_signature(table->signature, 4);
        printf("\n  Length: %u bytes\n", table->length);
        printf("  Revision: %u\n", table->revision);
        printf("  OEM ID: ");
        print_signature(table->oem_id, 6);
        printf("\n  OEM Table ID: ");
        print_signature(table->oem_table_id, 8);
        printf("\n  OEM Revision: 0x%x\n", table->oem_revision);

        // Count and display entries (32-bit pointers)
        const uint32_t entry_count =
                (table->length - sizeof(ACPI_SDTHeader)) / 4;
        printf("  Number of entries: %u\n", entry_count);

        uint32_t *entries = (uint32_t *)(table + 1);
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%08x\n", i, entries[i]);
        }
    }
}

static int map_and_test_acpi(void) {
    printf("Attempting to get ACPI RSDP from kernel...\n");

    // Allocate space for the RSDP in userspace
    ACPI_RSDP rsdp;

    // Call the syscall to copy RSDP data from kernel to userspace
    const SyscallResult result = anos_map_firmware_tables((uintptr_t)&rsdp);

    if (result != SYSCALL_OK) {
        printf("Failed to get ACPI RSDP! Error code: %d\n", result);
        return -1;
    }

    printf("Successfully received ACPI RSDP from kernel\n");

    // Parse the RSDP and follow it to the ACPI tables
    parse_acpi_rsdp(&rsdp);

    return 0;
}

int main(const int argc, char **argv) {
    printf("\nDEVMAN System device manager #%s [libanos #%s]\n\n", VERSION,
           libanos_version());

    // Test the firmware table mapping
    const int result = map_and_test_acpi();
    if (result != 0) {
        printf("ACPI table mapping test failed with code %d\n", result);
    } else {
        printf("\nACPI table mapping test completed successfully!\n");
    }

    printf("\nDevice manager initialization complete. Going into service "
           "loop...\n");

    while (1) {
        anos_task_sleep_current_secs(5);
    }
}