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

// ACPI region size - matches kernel definition (128KB)
#define ACPI_REGION_SIZE (128 * 1024)
#define USER_ACPI_BASE 0x8040000000 // Arbitrary userspace address for mapping

static void print_signature(const char *sig, size_t len) {
    for (size_t i = 0; i < len && sig[i] != '\0'; i++) {
        printf("%c", sig[i]);
    }
}

static bool has_signature(const char *expected, const char *actual,
                          size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            return false;
        }
    }
    return true;
}

static void parse_acpi_tables(void *acpi_base) {
    printf("Scanning for RSDP at user address 0x%lx...\n",
           (uintptr_t)acpi_base);

    ACPI_RSDP *rsdp = NULL;

    // Find the RSDP by scanning for the signature "RSD PTR "
    for (uintptr_t offset = 0; offset < ACPI_REGION_SIZE; offset += 16) {
        ACPI_RSDP *candidate = (ACPI_RSDP *)((uint8_t *)acpi_base + offset);

        if (has_signature("RSD PTR ", candidate->signature, 8)) {
            rsdp = candidate;
            printf("Found RSDP at offset 0x%lx\n", offset);
            break;
        }
    }

    if (!rsdp) {
        printf("RSDP not found in the mapped ACPI region.\n");
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

    // Use XSDT if available (revision >= 2), otherwise fall back to RSDT
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        // Try to access XSDT - convert physical address to our mapped region
        // This assumes the XSDT is within our mapped 128KB region
        const uint64_t xsdt_offset =
                rsdp->xsdt_address -
                0xFFFFFFFF81000000; // Convert from kernel virtual to offset

        if (xsdt_offset < ACPI_REGION_SIZE) {
            ACPI_SDTHeader *xsdt =
                    (ACPI_SDTHeader *)((uint8_t *)acpi_base + xsdt_offset);

            printf("\nUsing XSDT (64-bit system descriptor table):\n");
            printf("  Signature: ");
            print_signature(xsdt->signature, 4);
            printf("\n  Length: %u bytes\n", xsdt->length);
            printf("  Revision: %u\n", xsdt->revision);
            printf("  OEM ID: ");
            print_signature(xsdt->oem_id, 6);
            printf("\n  OEM Table ID: ");
            print_signature(xsdt->oem_table_id, 8);
            printf("\n  OEM Revision: 0x%x\n", xsdt->oem_revision);

            // Count and display entries (64-bit pointers)
            uint32_t entry_count = (xsdt->length - sizeof(ACPI_SDTHeader)) / 8;
            printf("  Number of entries: %u\n", entry_count);

            uint64_t *entries = (uint64_t *)(xsdt + 1);
            for (uint32_t i = 0; i < entry_count && i < 8; i++) {
                printf("  Entry %u: 0x%016lx\n", i, entries[i]);
            }
        } else {
            printf("XSDT address 0x%016lx is outside our mapped region.\n",
                   rsdp->xsdt_address);
        }
    } else if (rsdp->rsdt_address != 0) {
        // Use RSDT
        uint64_t rsdt_offset =
                rsdp->rsdt_address -
                0xFFFFFFFF81000000; // Convert from kernel virtual to offset

        if (rsdt_offset < ACPI_REGION_SIZE) {
            ACPI_SDTHeader *rsdt =
                    (ACPI_SDTHeader *)((uint8_t *)acpi_base + rsdt_offset);

            printf("\nUsing RSDT (32-bit system descriptor table):\n");
            printf("  Signature: ");
            print_signature(rsdt->signature, 4);
            printf("\n  Length: %u bytes\n", rsdt->length);
            printf("  Revision: %u\n", rsdt->revision);
            printf("  OEM ID: ");
            print_signature(rsdt->oem_id, 6);
            printf("\n  OEM Table ID: ");
            print_signature(rsdt->oem_table_id, 8);
            printf("\n  OEM Revision: 0x%x\n", rsdt->oem_revision);

            // Count and display entries (32-bit pointers)
            uint32_t entry_count = (rsdt->length - sizeof(ACPI_SDTHeader)) / 4;
            printf("  Number of entries: %u\n", entry_count);

            uint32_t *entries = (uint32_t *)(rsdt + 1);
            for (uint32_t i = 0; i < entry_count && i < 8; i++) {
                printf("  Entry %u: 0x%08x\n", i, entries[i]);
            }
        } else {
            printf("RSDT address 0x%08x is outside our mapped region.\n",
                   rsdp->rsdt_address);
        }
    } else {
        printf("No valid RSDT or XSDT address found in RSDP.\n");
    }
}

static int map_and_test_acpi(void) {
    printf("Attempting to map firmware tables...\n");

    // Try to map the ACPI tables
    const SyscallResult result = anos_map_firmware_tables(USER_ACPI_BASE);

    if (result != SYSCALL_OK) {
        printf("Failed to map firmware tables! Error code: %d\n", result);
        return -1;
    }

    void *acpi_tables = (void *)USER_ACPI_BASE;
    printf("Successfully mapped firmware tables at 0x%lx\n",
           (uintptr_t)acpi_tables);

    // Parse the tables
    parse_acpi_tables(acpi_tables);

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