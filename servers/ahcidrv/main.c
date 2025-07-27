/*
 * AHCI Driver Server
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "ahci.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static AHCIController controller = {0};
static AHCIPort ports[32] = {0};

static int ahci_initialize_driver(uint64_t ahci_base,
                                  uint64_t pci_config_base) {
#ifdef DEBUG_AHCI_INIT
    printf("Initializing AHCI driver:\n");
    printf("  AHCI Base: 0x%016lx\n", ahci_base);
    printf("  PCI Config Base: 0x%016lx\n", pci_config_base);
#endif

    if (!ahci_controller_init(&controller, ahci_base, pci_config_base)) {
        printf("Failed to initialize AHCI controller\n");
        return -1;
    }

#ifdef DEBUG_AHCI_INIT
    printf("AHCI controller initialized successfully\n");
    printf("  Port count: %u\n", controller.port_count);
    printf("  Active ports: 0x%08x\n", controller.active_ports);
#endif

    for (uint8_t i = 0; i < controller.port_count; i++) {
        if (controller.active_ports & (1 << i)) {
            if (ahci_port_init(&ports[i], &controller, i)) {
#ifdef DEBUG_AHCI_INIT
                printf("Port %u initialized successfully\n", i);
#endif
                if (ahci_port_identify(&ports[i])) {
#ifdef DEBUG_AHCI_INIT
                    printf("Port %u: Device identified - %lu sectors, %u "
                           "bytes/sector\n",
                           i, ports[i].sector_count, ports[i].sector_size);
#endif
                } else {
#ifdef DEBUG_AHCI_INIT
                    printf("Warning: Failed to identify device on port %u\n",
                           i);
#endif
                }
            } else {
#ifdef DEBUG_AHCI_INIT
                printf("Warning: Failed to initialize port %u\n", i);
#endif
            }
        }
    }

    return 0;
}

int main(const int argc, char **argv) {
    printf("\nAHCI Driver #%s [libanos #%s]", VERSION, libanos_version());

    if (argc < 3) {
        printf("\n\nUsage: %s <ahci_base> <pci_config_base>\n", argv[0]);
        printf("Arguments provided: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("  argv[%d]: %s\n", i, argv[i]);
        }
        return 1;
    }

    printf(" @ AHCI:0x%s PCI:0x%s\n", argv[1], argv[2]);

    const uint64_t ahci_base = strtoull(argv[1], nullptr, 16);
    const uint64_t pci_config_base = strtoull(argv[2], nullptr, 16);

    if (ahci_initialize_driver(ahci_base, pci_config_base) != 0) {
        printf("Failed to initialize AHCI driver\n");
        return 1;
    }

    printf("AHCI initialization @ 0x%s complete.\n", argv[1]);

    while (1) {
        anos_task_sleep_current_secs(5);
    }

    ahci_controller_cleanup(&controller);
    return 0;
}