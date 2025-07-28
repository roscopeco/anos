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

typedef struct {
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
    uint32_t reserved;
} __attribute__((packed)) MCFG_Entry;

typedef struct {
    ACPI_SDTHeader header;
    uint64_t reserved;
} __attribute__((packed)) MCFG_Table;

// ACPI constants
#define USER_ACPI_BASE 0x8040000000 // Base for mapping ACPI tables

// Process spawning constants
#define PROCESS_SPAWN ((1))

typedef struct {
    uint64_t capability_id;
    uint64_t capability_cookie;
} InitCapability;

typedef struct {
    uint64_t stack_size;
    uint16_t argc;
    uint16_t capc;
    char data[];
} ProcessSpawnRequest;

#ifdef DEBUG_ACPI
static void print_signature(const char *sig, const size_t len) {
    for (size_t i = 0; i < len && sig[i] != '\0'; i++) {
        printf("%c", sig[i]);
    }
}
#endif

extern uint64_t __syscall_capabilities[];

static ACPI_SDTHeader *find_acpi_table(const char *signature,
                                       const uint64_t *entries,
                                       const uint32_t entry_count) {
    for (uint32_t i = 0; i < entry_count; i++) {
        const uint64_t table_addr = entries[i];

        const uint64_t table_phys_page = table_addr & ~0xFFF;
        const uint64_t table_offset = table_addr & 0xFFF;

        const uintptr_t temp_base = USER_ACPI_BASE + 0x10000 + (i * 0x1000);
        const SyscallResult result =
                anos_map_physical(table_phys_page, (void *)temp_base, 4096,
                                  ANOS_MAP_VIRTUAL_FLAG_READ);

        if (result.result == SYSCALL_OK) {
            ACPI_SDTHeader *table =
                    (ACPI_SDTHeader *)((uint8_t *)temp_base + table_offset);

            if (strncmp(table->signature, signature, 4) == 0) {
                return table;
            }
        }
    }

    return nullptr;
}

static int64_t spawn_process_via_system(const uint64_t stack_size,
                                        const uint16_t capc,
                                        const InitCapability *capabilities,
                                        const uint16_t argc,
                                        const char *argv[]) {
    const SyscallResult result = anos_find_named_channel("SYSTEM::PROCESS");

    const uint64_t system_process_channel = result.value;

    if (result.result != SYSCALL_OK || !system_process_channel) {
        printf("ERROR: Could not find SYSTEM::PROCESS channel\n");
        return -1;
    }

    size_t capabilities_size = capc * sizeof(InitCapability);
    size_t argv_size = 0;

    for (uint16_t i = 0; i < argc; i++) {
        if (argv[i]) {
            argv_size += strlen(argv[i]) + 1;
        }
    }

    size_t total_size =
            sizeof(ProcessSpawnRequest) + capabilities_size + argv_size;

    // Use page-aligned buffer for IPC (required by kernel)
    static char __attribute__((aligned(4096))) ipc_buffer[4096];
    char *buffer = ipc_buffer;

    if (total_size > sizeof(ipc_buffer)) {
        printf("ERROR: Message too large (%zu > %zu)\n", total_size,
               sizeof(ipc_buffer));
        return -2;
    }

    ProcessSpawnRequest *req = (ProcessSpawnRequest *)buffer;
    req->stack_size = stack_size;
    req->argc = argc;
    req->capc = capc;

    char *data_ptr = req->data;

    if (capc > 0 && capabilities) {
        memcpy(data_ptr, capabilities, capabilities_size);
        data_ptr += capabilities_size;
    }

    if (argc > 0 && argv) {
        for (uint16_t i = 0; i < argc; i++) {
            if (argv[i]) {
                size_t len = strlen(argv[i]);
                memcpy(data_ptr, argv[i], len);
                data_ptr[len] = '\0';
                data_ptr += len + 1;
            }
        }
    }

#ifdef DEBUG_PCI
    printf("Sending process spawn request (total_size=%ld)\n", total_size);
#endif

    SyscallResult response = anos_send_message(
            system_process_channel, PROCESS_SPAWN, total_size, buffer);

    if (response.result == SYSCALL_OK) {
        return (int64_t)response.value;
    }

    return (int64_t)response.result;
}

static void spawn_pci_bus_driver(const MCFG_Entry *entry) {
#ifdef DEBUG_PCI
    printf("Spawning PCI bus driver for segment %u, buses %u-%u...\n",
           entry->pci_segment_group, entry->start_bus_number,
           entry->end_bus_number);
#endif

    // Prepare arguments for the PCI driver
    char ecam_base_str[32];
    char segment_str[16];
    char bus_start_str[16];
    char bus_end_str[16];

    snprintf(ecam_base_str, sizeof(ecam_base_str), "%lx", entry->base_address);
    snprintf(segment_str, sizeof(segment_str), "%u", entry->pci_segment_group);
    snprintf(bus_start_str, sizeof(bus_start_str), "%u",
             entry->start_bus_number);
    snprintf(bus_end_str, sizeof(bus_end_str), "%u", entry->end_bus_number);

    const char *argv[] = {"boot:/pcidrv.elf", ecam_base_str, segment_str,
                          bus_start_str, bus_end_str};

    const InitCapability pci_caps[] = {
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT],
                    .capability_id = SYSCALL_ID_DEBUG_PRINT,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR],
                    .capability_id = SYSCALL_ID_DEBUG_CHAR,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_SLEEP],
                    .capability_id = SYSCALL_ID_SLEEP,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL],
                    .capability_id = SYSCALL_ID_MAP_PHYSICAL,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL],
                    .capability_id = SYSCALL_ID_MAP_VIRTUAL,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_ALLOC_PHYSICAL_PAGES],
                    .capability_id = SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE],
                    .capability_id = SYSCALL_ID_SEND_MESSAGE,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_FIND_NAMED_CHANNEL],
                    .capability_id = SYSCALL_ID_FIND_NAMED_CHANNEL,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_KILL_CURRENT_TASK],
                    .capability_id = SYSCALL_ID_KILL_CURRENT_TASK,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_ALLOC_INTERRUPT_VECTOR],
                    .capability_id = SYSCALL_ID_ALLOC_INTERRUPT_VECTOR,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_WAIT_INTERRUPT],
                    .capability_id = SYSCALL_ID_WAIT_INTERRUPT,
            },
    };

#ifdef DEBUG_PCI
    printf("  --> spawn: %s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3],
           argv[4]);
#endif

    int64_t pid = spawn_process_via_system(0x100000, 11, pci_caps, 5, argv);
    if (pid > 0) {
#ifdef DEBUG_PCI
        printf("  --> PCI driver spawned with PID %ld\n", pid);
#endif
    } else {
        printf("ERROR: Failed to spawn PCI driver (error code: %ld)\n", pid);
    }
}

static void parse_mcfg_table(ACPI_SDTHeader *mcfg_header) {
    if (!mcfg_header) {
#ifdef DEBUG_PCI_MCFG
        printf("No MCFG table found\n");
#endif
        return;
    }

#ifdef DEBUG_PCI_MCFG
    printf("\n--- MCFG (PCI Express Memory Configuration Space) Table ---\n");
    printf("Signature: ");
    print_signature(mcfg_header->signature, 4);
    printf("\nLength: %u bytes\n", mcfg_header->length);
    printf("Revision: %u\n", mcfg_header->revision);
    printf("OEM ID: ");
    print_signature(mcfg_header->oem_id, 6);
    printf("\nOEM Table ID: ");
    print_signature(mcfg_header->oem_table_id, 8);
    printf("\n");
#endif

    MCFG_Table *mcfg = (MCFG_Table *)mcfg_header;

    const uint32_t entries_size = mcfg->header.length - sizeof(MCFG_Table);
    uint32_t num_entries = entries_size / sizeof(MCFG_Entry);

#ifdef DEBUG_PCI_MCFG
    printf("Number of PCI host bridge entries: %u\n", num_entries);
#endif

    if (num_entries == 0) {
#ifdef DEBUG_PCI_MCFG
        printf("No PCI host bridge entries found\n");
#endif
        return;
    }

    const MCFG_Entry *entries = (MCFG_Entry *)(mcfg + 1);

    for (uint32_t i = 0; i < num_entries; i++) {
#ifdef DEBUG_PCI_MCFG
        printf("\nPCI Host Bridge %u:\n", i);
        printf("  Base Address: 0x%016lx\n", entries[i].base_address);
        printf("  PCI Segment Group: %u\n", entries[i].pci_segment_group);
        printf("  Start Bus Number: %u\n", entries[i].start_bus_number);
        printf("  End Bus Number: %u\n", entries[i].end_bus_number);
        printf("  Bus Range: %u-%u (%u buses)\n", entries[i].start_bus_number,
               entries[i].end_bus_number,
               entries[i].end_bus_number - entries[i].start_bus_number + 1);
#endif
        // Launch PCI bus driver for this host bridge
        spawn_pci_bus_driver(&entries[i]);
    }
}

static void parse_acpi_rsdp(ACPI_RSDP *rsdp) {
#ifdef DEBUG_ACPI
    printf("Parsing ACPI RSDP at 0x%lx...\n", (uintptr_t)rsdp);
#endif

    if (!rsdp) {
#ifdef DEBUG_ACPI
        printf("No RSDP provided.\n");
#endif
        return;
    }

#ifdef DEBUG_ACPI
    printf("\nRSDP Information:\n");
    printf("  Signature: ");
    print_signature(rsdp->signature, 8);
    printf("\n  OEM ID: ");
    print_signature(rsdp->oem_id, 6);
    printf("\n  Revision: %u\n", rsdp->revision);
    printf("  RSDT Address: 0x%08x\n", rsdp->rsdt_address);
    printf("  XSDT Address: 0x%016lx\n", rsdp->xsdt_address);
#endif

    uint64_t table_addr = 0;
    bool use_xsdt = false;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        table_addr = rsdp->xsdt_address;
        use_xsdt = true;
#ifdef DEBUG_ACPI
        printf("\nWill use XSDT at physical address 0x%016lx\n", table_addr);
#endif
    } else if (rsdp->rsdt_address != 0) {
        table_addr = rsdp->rsdt_address;
        use_xsdt = false;
#ifdef DEBUG_ACPI
        printf("\nWill use RSDT at physical address 0x%08x\n",
               (uint32_t)table_addr);
#endif
    } else {
#ifdef DEBUG_ACPI
        printf("No valid RSDT or XSDT address found in RSDP.\n");
#endif
        return;
    }

    // Map the table (align address down to page boundary and map one page)
    const uint64_t table_phys_page = table_addr & ~0xFFF;
    const uint64_t table_offset = table_addr & 0xFFF;

#ifdef DEBUG_ACPI
    printf("Mapping physical page 0x%016lx to user address 0x%lx\n",
           table_phys_page, USER_ACPI_BASE);
#endif

    // Map the physical page containing the table
    const SyscallResult result =
            anos_map_physical(table_phys_page, (void *)USER_ACPI_BASE, 4096,
                              ANOS_MAP_VIRTUAL_FLAG_READ);
    if (result.result != SYSCALL_OK) {
        printf("Failed to map ACPI table! Error code: %ld\n", result.result);
        return;
    }

    // Access the table at the correct offset within the mapped page
    ACPI_SDTHeader *table =
            (ACPI_SDTHeader *)((uint8_t *)USER_ACPI_BASE + table_offset);

    if (use_xsdt) {
#ifdef DEBUG_ACPI
        printf("\nXSDT (64-bit system descriptor table) at 0x%lx:\n",
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
#endif

        // Count and display entries (64-bit pointers)
        const uint32_t entry_count =
                (table->length - sizeof(ACPI_SDTHeader)) / 8;
#ifdef DEBUG_ACPI
        printf("  Number of entries: %u\n", entry_count);
#endif

        const uint64_t *entries = (uint64_t *)(table + 1);
#ifdef DEBUG_ACPI
        for (uint32_t i = 0; i < entry_count && i < 8; i++) {
            printf("  Entry %u: 0x%016lx\n", i, entries[i]);
        }
#endif

#ifdef DEBUG_ACPI
#ifdef VERY_NOISY_ACPI
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
                    first_table_phys_page, (void *)first_table_base, 4096,
                    ANOS_MAP_VIRTUAL_FLAG_READ);
            if (table_result.result != SYSCALL_OK) {
                printf("Failed to map first table! Error code: %ld\n",
                       table_result.result);
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
            uint64_t sig_table_addr = entries[i];

            // Map each table to read its signature
            uint64_t sig_table_phys_page = sig_table_addr & ~0xFFF;
            uint64_t sig_table_offset = sig_table_addr & 0xFFF;

            const uintptr_t sig_table_base =
                    USER_ACPI_BASE + 0x2000 + (i * 0x1000);
            const SyscallResult sig_result = anos_map_physical(
                    sig_table_phys_page, (void *)sig_table_base, 4096,
                    ANOS_MAP_VIRTUAL_FLAG_READ);
            if (sig_result.result == SYSCALL_OK) {
                ACPI_SDTHeader *sig_table =
                        (ACPI_SDTHeader *)((uint8_t *)sig_table_base +
                                           sig_table_offset);
                printf("  Table %u: ", i);
                print_signature(sig_table->signature, 4);
                printf("\n");
            } else {
                printf("  Table %u: <mapping failed>\n", i);
            }
        }

        printf("\n--- Searching for MCFG Table ---\n");
#endif
#endif

        // Look for and parse MCFG table
        ACPI_SDTHeader *mcfg_table =
                find_acpi_table("MCFG", entries, entry_count);

        if (!mcfg_table) {
            printf("WARN: Failed to find PCIe advanced configuration "
                   "information, and legacy PCI is not supported!\n");
            return;
        }

        parse_mcfg_table(mcfg_table);
    } else {
#ifdef DEBUG_ACPI
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
#endif
    }
}

static int map_and_init_acpi(void) {
#ifdef DEBUG_ACPI
    printf("Attempting to get ACPI RSDP from kernel...\n");
#endif
    // Allocate space for the RSDP in userspace
    ACPI_RSDP rsdp;

    // Call the syscall to copy RSDP data from kernel to userspace
    const SyscallResult result = anos_map_firmware_tables((uintptr_t)&rsdp);

    if (result.result != SYSCALL_OK) {
        printf("Failed to get ACPI RSDP! Error code: %ld\n", result.result);
        return -1;
    }

#ifdef DEBUG_ACPI
    printf("Successfully received ACPI RSDP from kernel\n");
#endif

    // Parse the RSDP and follow it to the ACPI tables
    parse_acpi_rsdp(&rsdp);

    return 0;
}

int main(const int argc, char **argv) {
    printf("\nDEVMAN System device manager #%s [libanos #%s]\n", VERSION,
           libanos_version());

    // Test the firmware table mapping
    const int result = map_and_init_acpi();
    if (result != 0) {
#ifdef DEBUG_ACPI
        printf("ACPI table mapping failed with code %d\n", result);
#endif
    } else {
#ifdef DEBUG_ACPI
        printf("\nACPI table mapping completed successfully!\n");
#endif
    }

    printf("Device manager initialization complete.\n");

    while (1) {
        anos_task_sleep_current_secs(5);
    }
}