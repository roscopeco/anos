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

// RSDP page size
#define RSDP_PAGE_SIZE 4096
#define USER_RSDP_BASE                                                         \
    0x8040000000 // Arbitrary userspace address for RSDP mapping
#define USER_ACPI_BASE 0x8050000000 // Base for mapping other ACPI tables

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

static void parse_acpi_rsdp(void *rsdp_page) {
    printf("Scanning for RSDP at user address 0x%lx...\n",
           (uintptr_t)rsdp_page);

    ACPI_RSDP *rsdp = NULL;

    // Find the RSDP by scanning for the signature "RSD PTR "
    for (uintptr_t offset = 0; offset < RSDP_PAGE_SIZE; offset += 16) {
        ACPI_RSDP *candidate = (ACPI_RSDP *)((uint8_t *)rsdp_page + offset);

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

    // Now we need to map the XSDT/RSDT using the new map_physical_memory syscall
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
    uint64_t table_phys_page = table_addr & ~0xFFF;
    uint64_t table_offset = table_addr & 0xFFF;

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
        uint32_t entry_count = (table->length - sizeof(ACPI_SDTHeader)) / 8;
        printf("  Number of entries: %u\n", entry_count);

        uint64_t *entries = (uint64_t *)(table + 1);
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%016lx\n", i, entries[i]);
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
    printf("Attempting to map RSDP...\n");

    // Try to map the RSDP page
    const SyscallResult result = anos_map_firmware_tables(USER_RSDP_BASE);

    if (result != SYSCALL_OK) {
        printf("Failed to map RSDP! Error code: %d\n", result);
        return -1;
    }

    void *rsdp_page = (void *)USER_RSDP_BASE;
    printf("Successfully mapped RSDP at 0x%lx\n", (uintptr_t)rsdp_page);

    // Parse the RSDP and follow it to the ACPI tables
    parse_acpi_rsdp(rsdp_page);

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