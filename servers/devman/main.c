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

// ACPI region constants (matches kernel definitions)
#define ACPI_TABLES_VADDR_BASE 0xFFFFFFFF81000000
#define ACPI_TABLES_VADDR_LIMIT 0xFFFFFFFF81020000
#define ACPI_REGION_SIZE (ACPI_TABLES_VADDR_LIMIT - ACPI_TABLES_VADDR_BASE)
#define USER_ACPI_BASE 0x8040000000 // Userspace base for entire ACPI region

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

static void parse_acpi_rsdp(void *acpi_region) {
    printf("Scanning for RSDP in ACPI region at user address 0x%lx...\n",
           (uintptr_t)acpi_region);

    ACPI_RSDP *rsdp = NULL;

    // Find the RSDP by scanning the entire ACPI region for the signature "RSD PTR "
    for (uintptr_t offset = 0; offset < ACPI_REGION_SIZE; offset += 16) {
        ACPI_RSDP *candidate = (ACPI_RSDP *)((uint8_t *)acpi_region + offset);

        if (has_signature("RSD PTR ", candidate->signature, 8)) {
            rsdp = candidate;
            printf("Found RSDP at offset 0x%lx\n", offset);
            break;
        }
    }

    if (!rsdp) {
        printf("RSDP not found in the mapped page.\n");
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

    // With the new handover approach, all ACPI tables are directly accessible
    // in the mapped region. We need to find the RSDT/XSDT by its signature.
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

    // Since the kernel has already handed over the tables and converted virtual
    // addresses back to physical, we can find the table by scanning for its signature
    ACPI_SDTHeader *table = NULL;

    // Scan the ACPI region to find the table with the correct signature
    for (uintptr_t offset = 0; offset < ACPI_REGION_SIZE; offset += 16) {
        void *candidate_ptr = (uint8_t *)acpi_region + offset;
        ACPI_SDTHeader *candidate = (ACPI_SDTHeader *)candidate_ptr;

        // Check if this looks like the table we're looking for
        if ((use_xsdt && has_signature("XSDT", candidate->signature, 4)) ||
            (!use_xsdt && has_signature("RSDT", candidate->signature, 4))) {
            table = candidate;
            printf("Found %s table in mapped region at offset 0x%lx\n",
                   use_xsdt ? "XSDT" : "RSDT", offset);
            break;
        }
    }

    if (!table) {
        printf("Could not find %s table in mapped ACPI region\n",
               use_xsdt ? "XSDT" : "RSDT");
        return;
    }

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
        uint32_t entry_count = (table->length - sizeof(ACPI_SDTHeader)) / 8;
        printf("  Number of entries: %u\n", entry_count);

        uint64_t *entries = (uint64_t *)(table + 1);
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%016lx\n", i, entries[i]);
        }

        // Let's dump the first table as a test
        if (entry_count > 0) {
            printf("\n--- Dumping first ACPI table as test ---\n");
            uint64_t first_table_addr = entries[0];

            printf("Looking for first table at physical 0x%016lx\n",
                   first_table_addr);

            // Since all ACPI tables are now mapped in our region, find it by scanning
            ACPI_SDTHeader *first_table = NULL;
            for (uintptr_t offset = 0; offset < ACPI_REGION_SIZE;
                 offset += 16) {
                void *candidate_ptr = (uint8_t *)acpi_region + offset;
                ACPI_SDTHeader *candidate = (ACPI_SDTHeader *)candidate_ptr;

                // Check if this is a valid ACPI table header (has proper signature format)
                if (candidate->signature[0] >= 'A' &&
                    candidate->signature[0] <= 'Z' &&
                    candidate->signature[1] >= 'A' &&
                    candidate->signature[1] <= 'Z' &&
                    candidate->signature[2] >= 'A' &&
                    candidate->signature[2] <= 'Z' &&
                    candidate->signature[3] >= 'A' &&
                    candidate->signature[3] <= 'Z' &&
                    candidate->length >= sizeof(ACPI_SDTHeader) &&
                    candidate->length < 0x100000) {
                    // Found a potential table - for now, let's use the first one we find
                    first_table = candidate;
                    printf("Found potential ACPI table at offset 0x%lx with "
                           "signature: ",
                           offset);
                    print_signature(first_table->signature, 4);
                    printf("\n");
                    break;
                }
            }

            if (first_table) {

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
                uint8_t *table_data = (uint8_t *)(first_table + 1);
                uint32_t data_size =
                        first_table->length - sizeof(ACPI_SDTHeader);
                uint32_t dump_size = data_size < 64 ? data_size : 64;

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
            } else {
                printf("Could not find any valid ACPI table in the mapped "
                       "region\n");
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
        uint32_t entry_count = (table->length - sizeof(ACPI_SDTHeader)) / 4;
        printf("  Number of entries: %u\n", entry_count);

        uint32_t *entries = (uint32_t *)(table + 1);
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%08x\n", i, entries[i]);
        }
    }
}

static int map_and_test_acpi(void) {
    printf("Attempting to map ACPI tables region...\n");

    // Try to map the entire ACPI tables region
    const SyscallResult result = anos_map_firmware_tables(USER_ACPI_BASE);

    if (result != SYSCALL_OK) {
        printf("Failed to map ACPI tables! Error code: %d\n", result);
        return -1;
    }

    void *acpi_region = (void *)USER_ACPI_BASE;
    printf("Successfully mapped ACPI region (%ld KB) at 0x%lx\n",
           ACPI_REGION_SIZE / 1024, (uintptr_t)acpi_region);

    // Parse the RSDP and follow it to the ACPI tables
    parse_acpi_rsdp(acpi_region);

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