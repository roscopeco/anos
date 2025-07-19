/*
 * PCI Bus Driver Server
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "enumerate.h"
#include "pci.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static PCIBusDriver bus_driver = {0};

static int pci_initialize_driver(uint64_t ecam_base, const uint16_t segment,
                                 const uint8_t bus_start,
                                 const uint8_t bus_end) {
#ifdef DEBUG_BUS_DRIVER_INIT
    printf("Initializing PCI bus driver:\n");
    printf("  ECAM Base: 0x%016lx\n", ecam_base);
    printf("  Segment: %u\n", segment);
    printf("  Bus Range: %u-%u\n", bus_start, bus_end);
#endif

    bus_driver.ecam_base = ecam_base;
    bus_driver.segment = segment;
    bus_driver.bus_start = bus_start;
    bus_driver.bus_end = bus_end;

    // Calculate size needed (1MB per bus)
    const uint32_t num_buses = bus_end - bus_start + 1;
    bus_driver.mapped_size = num_buses * 1024 * 1024;

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("  Mapping %lu MB of ECAM space...\n",
           bus_driver.mapped_size / (1024 * 1024));
#endif

    // Map the ECAM space
    const SyscallResult result = anos_map_physical(
            ecam_base, (void *)0x9000000000, bus_driver.mapped_size);
    if (result != SYSCALL_OK) {
        printf("Failed to map ECAM space! Error: %d\n", result);
        return -1;
    }

    bus_driver.mapped_ecam = (void *)0x9000000000;

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("ECAM mapping successful at virtual address 0x%lx\n",
           (uintptr_t)bus_driver.mapped_ecam);
#endif

    return 0;
}

static void pci_enumerate_all_buses(void) {
    // TODO maybe don't run through all the buses?
    //      should only have one root bus, can recursively scan from there...
    for (uint16_t bus = bus_driver.bus_start; bus <= bus_driver.bus_end;
         bus++) {
        pci_enumerate_bus(&bus_driver, bus);
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

    const uint64_t ecam_base = strtoull(argv[1], nullptr, 16);
    const uint16_t segment = (uint16_t)strtoul(argv[2], nullptr, 10);
    const uint8_t bus_start = (uint8_t)strtoul(argv[3], nullptr, 10);
    const uint8_t bus_end = (uint8_t)strtoul(argv[4], nullptr, 10);

    if (bus_start > bus_end) {
        printf("Invalid bus range: %u-%u\n", bus_start, bus_end);
        return 1;
    }

    if (pci_initialize_driver(ecam_base, segment, bus_start, bus_end) != 0) {
        printf("Failed to initialize PCI driver\n");
        return 1;
    }

    pci_enumerate_all_buses();

    printf("\nPCI enumeration complete, PCI driver exiting.\n");
    return 0;
}