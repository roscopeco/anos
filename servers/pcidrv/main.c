/*
 * PCI Bus Driver Server
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

// PCI Configuration Space offsets
#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_STATUS 0x06
#define PCI_REVISION_ID 0x08
#define PCI_CLASS_CODE 0x09
#define PCI_HEADER_TYPE 0x0E
#define PCI_SUBSYSTEM_ID 0x2E

// PCI Header Types
#define PCI_HEADER_TYPE_NORMAL 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// PCI Configuration Space layout
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[6];
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base;
    uint8_t capabilities_pointer;
    uint8_t reserved[7];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} __attribute__((packed)) PCIConfigSpace;

// Bus driver state
typedef struct {
    uint64_t ecam_base;
    uint16_t segment;
    uint8_t bus_start;
    uint8_t bus_end;
    void *mapped_ecam;
    size_t mapped_size;
} PCIBusDriver;

static PCIBusDriver bus_driver = {0};

static uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function,
                                  uint8_t offset) {
    if (bus < bus_driver.bus_start || bus > bus_driver.bus_end) {
        return 0xFFFFFFFF;
    }

    if (!bus_driver.mapped_ecam) {
        return 0xFFFFFFFF;
    }

    uint64_t device_offset = ((uint64_t)(bus - bus_driver.bus_start) << 20) |
                             ((uint64_t)device << 15) |
                             ((uint64_t)function << 12) | offset;

    const uint32_t *config_addr =
            (uint32_t *)((uint8_t *)bus_driver.mapped_ecam + device_offset);
    return *config_addr;
}

static uint16_t pci_config_read16(const uint8_t bus, const uint8_t device,
                                  const uint8_t function,
                                  const uint8_t offset) {
    const uint32_t dword =
            pci_config_read32(bus, device, function, offset & ~3);
    return (uint16_t)(dword >> ((offset & 3) * 8));
}

static uint8_t pci_config_read8(const uint8_t bus, const uint8_t device,
                                const uint8_t function, const uint8_t offset) {
    const uint32_t dword =
            pci_config_read32(bus, device, function, offset & ~3);
    return (uint8_t)(dword >> ((offset & 3) * 8));
}

static bool pci_device_exists(const uint8_t bus, const uint8_t device,
                              const uint8_t function) {
    const uint16_t vendor_id =
            pci_config_read16(bus, device, function, PCI_VENDOR_ID);
    return vendor_id != 0xFFFF && vendor_id != 0x0000;
}

static void pci_enumerate_function(const uint8_t bus, const uint8_t device,
                                   const uint8_t function) {
    if (!pci_device_exists(bus, device, function)) {
        return;
    }

    const uint16_t vendor_id =
            pci_config_read16(bus, device, function, PCI_VENDOR_ID);
    const uint16_t device_id =
            pci_config_read16(bus, device, function, PCI_DEVICE_ID);
    const uint8_t class_code =
            pci_config_read8(bus, device, function, PCI_CLASS_CODE);
    const uint8_t subclass =
            pci_config_read8(bus, device, function, PCI_CLASS_CODE - 1);
    const uint8_t prog_if =
            pci_config_read8(bus, device, function, PCI_CLASS_CODE - 2);
    const uint8_t header_type =
            pci_config_read8(bus, device, function, PCI_HEADER_TYPE);

    printf("PCI %02x:%02x.%x - Vendor: 0x%04x Device: 0x%04x Class: "
           "%02x.%02x.%02x",
           bus, device, function, vendor_id, device_id, class_code, subclass,
           prog_if);

    // Check if it's a bridge
    if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
        const uint8_t secondary_bus =
                pci_config_read8(bus, device, function, 0x19);
        printf(" [Bridge to bus %02x]", secondary_bus);

        // TODO: Recursively enumerate secondary bus
    }

    printf("\n");
}

static void pci_enumerate_device(const uint8_t bus, const uint8_t device) {
    if (!pci_device_exists(bus, device, 0)) {
        return;
    }

    const uint8_t header_type =
            pci_config_read8(bus, device, 0, PCI_HEADER_TYPE);

    // Enumerate function 0
    pci_enumerate_function(bus, device, 0);

    // If it's a multi-function device, enumerate other functions
    if (header_type & 0x80) {
        for (uint8_t function = 1; function < 8; function++) {
            pci_enumerate_function(bus, device, function);
        }
    }
}

static void pci_enumerate_bus(uint8_t bus) {
    printf("Enumerating PCI bus %02x...\n", bus);

    for (uint8_t device = 0; device < 32; device++) {
        pci_enumerate_device(bus, device);
    }
}

static int pci_initialize_driver(uint64_t ecam_base, const uint16_t segment,
                                 const uint8_t bus_start,
                                 const uint8_t bus_end) {
    printf("Initializing PCI bus driver:\n");
    printf("  ECAM Base: 0x%016lx\n", ecam_base);
    printf("  Segment: %u\n", segment);
    printf("  Bus Range: %u-%u\n", bus_start, bus_end);

    bus_driver.ecam_base = ecam_base;
    bus_driver.segment = segment;
    bus_driver.bus_start = bus_start;
    bus_driver.bus_end = bus_end;

    // Calculate size needed (1MB per bus)
    const uint32_t num_buses = bus_end - bus_start + 1;
    bus_driver.mapped_size = num_buses * 1024 * 1024;

    printf("  Mapping %lu MB of ECAM space...\n",
           bus_driver.mapped_size / (1024 * 1024));

    // Map the ECAM space
    const SyscallResult result = anos_map_physical(
            ecam_base, (void *)0x9000000000, bus_driver.mapped_size);
    if (result != SYSCALL_OK) {
        printf("Failed to map ECAM space! Error: %d\n", result);
        return -1;
    }

    bus_driver.mapped_ecam = (void *)0x9000000000;

    printf("ECAM mapping successful at virtual address 0x%lx\n",
           (uintptr_t)bus_driver.mapped_ecam);

    return 0;
}

static void pci_enumerate_all_buses(void) {
    for (uint8_t bus = bus_driver.bus_start; bus <= bus_driver.bus_end; bus++) {
        pci_enumerate_bus(bus);
    }
}

int main(const int argc, char **argv) {
    printf("\nPCI Bus Driver #%s [libanos #%s]\n\n", VERSION,
           libanos_version());

    if (argc < 5) {
        printf("Usage: %s <ecam_base> <segment> <bus_start> <bus_end>\n",
               argv[0]);
        printf("Arguments provided: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("  argv[%d]: %s\n", i, argv[i]);
        }
        return 1;
    }

    // Parse arguments
    const uint64_t ecam_base = strtoull(argv[1], NULL, 16);
    const uint16_t segment = (uint16_t)strtoul(argv[2], NULL, 10);
    const uint8_t bus_start = (uint8_t)strtoul(argv[3], NULL, 10);
    const uint8_t bus_end = (uint8_t)strtoul(argv[4], NULL, 10);

    // Validate bus range
    if (bus_start > bus_end) {
        printf("Invalid bus range: %u-%u\n", bus_start, bus_end);
        return 1;
    }

    // Initialize the driver
    if (pci_initialize_driver(ecam_base, segment, bus_start, bus_end) != 0) {
        printf("Failed to initialize PCI driver\n");
        return 1;
    }

    // Enumerate all devices on all buses in our range
    pci_enumerate_all_buses();

    printf("\nPCI enumeration complete. Entering service loop...\n");

    // TODO: Enter IPC service loop to handle requests from other drivers
    while (1) {
        anos_task_sleep_current_secs(10);
    }

    return 0;
}