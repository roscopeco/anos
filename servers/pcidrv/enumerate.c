/*
 * pcidrv - PCI enumeration routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pci.h"

void spawn_ahci_driver(uint64_t ahci_base, uint64_t pci_config_base);

bool pci_device_exists(const PCIBusDriver *bus_driver, const uint8_t bus,
                       const uint8_t device, const uint8_t function) {
    const uint16_t vendor_id =
            pci_config_read16(bus_driver, bus, device, function, PCI_VENDOR_ID);
    return vendor_id != 0xFFFF && vendor_id != 0x0000;
}

void pci_enumerate_function(const PCIBusDriver *bus_driver, const uint8_t bus,
                            const uint8_t device, const uint8_t function) {

    if (!pci_device_exists(bus_driver, bus, device, function)) {
        return;
    }

    const uint8_t header_type = pci_config_read8(bus_driver, bus, device,
                                                 function, PCI_HEADER_TYPE);

#ifdef DEBUG_BUS_DRIVER_ENUM
    const uint16_t vendor_id =
            pci_config_read16(bus_driver, bus, device, function, PCI_VENDOR_ID);

    const uint16_t device_id =
            pci_config_read16(bus_driver, bus, device, function, PCI_DEVICE_ID);
#endif

    const uint32_t class_d = pci_config_read32(bus_driver, bus, device,
                                               function, PCI_CLASS_CODE);
    const uint8_t class_code = PCI_REG_UU_B(class_d);
    const uint8_t subclass = PCI_REG_UM_B(class_d);
    const uint8_t prog_if = PCI_REG_LM_B(class_d);

#ifdef DEBUG_BUS_DRIVER_ENUM
    printf("PCI %02x:%02x.%x - Vendor: 0x%04x Device: 0x%04x Class: "
           "%02x.%02x.%02x",
           bus, device, function, vendor_id, device_id, class_code, subclass,
           prog_if);
#endif

    // Check for AHCI controller
    if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {

        // For AHCI, the ABAR (AHCI Base Address Register) is BAR5 at offset 0x24
        const uint32_t bar5_low =
                pci_config_read32(bus_driver, bus, device, function, 0x24);
#ifdef DEBUG_BUS_DRIVER_ENUM
        const uint32_t bar5_high =
                pci_config_read32(bus_driver, bus, device, function, 0x28);

#ifdef VERY_NOISY_BUS_DRIVER
        printf("\nDEBUG: BAR5 raw values: low=0x%08x high=0x%08x\n", bar5_low,
               bar5_high);
#endif
#endif

        if (bar5_low != 0 && bar5_low != 0xFFFFFFFF) {
            uint64_t ahci_base = (bar5_low & 0xFFFFFFF0);

#ifdef DEBUG_BUS_DRIVER_ENUM
            // Check if it's a 64-bit BAR
            if ((bar5_low & 0x6) == 0x4) {
                ahci_base |= ((uint64_t)bar5_high << 32);
#ifdef VERY_NOISY_BUS_DRIVER
                printf("DEBUG: 64-bit BAR detected\n");
#endif
            } else {
#ifdef VERY_NOISY_BUS_DRIVER
                printf("DEBUG: 32-bit BAR detected\n");
#endif
            }

            printf(" [AHCI Controller - Base: 0x%016lx]", ahci_base);
#else
            printf("Found: AHCI Controller at 0x%016lx; Starting "
                   "driver...\n",
                   ahci_base);
#endif

            // Calculate PCI configuration space base for this device
            const uint64_t pci_config_base =
                    bus_driver->ecam_base + ((uint64_t)bus << 20) +
                    ((uint64_t)device << 15) + ((uint64_t)function << 12);

            spawn_ahci_driver(ahci_base, pci_config_base);
        }
    }

    // Check if it's a bridge
    if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
#ifdef DEBUG_BUS_DRIVER_ENUM
        const uint8_t secondary_bus =
                pci_config_read8(bus_driver, bus, device, function, 0x19);
        printf(" [Bridge to bus %02x]", secondary_bus);
#endif

        // TODO: Recursively enumerate secondary bus
    }

#ifdef DEBUG_BUS_DRIVER_ENUM
    printf("\n");
#endif
}

void pci_enumerate_device(const PCIBusDriver *bus_driver, const uint8_t bus,
                          const uint8_t device) {
    if (!pci_device_exists(bus_driver, bus, device, 0)) {
        return;
    }

    const uint8_t header_type =
            pci_config_read8(bus_driver, bus, device, 0, PCI_HEADER_TYPE);

    // Enumerate function 0
    pci_enumerate_function(bus_driver, bus, device, 0);

    // If it's a multi-function device, enumerate other functions
    if (header_type & 0x80) {
        for (uint8_t function = 1; function < 8; function++) {
            pci_enumerate_function(bus_driver, bus, device, function);
        }
    }
}

void pci_enumerate_bus(const PCIBusDriver *bus_driver, const uint8_t bus) {
#ifdef VERY_NOISY_BUS_DRIVER
    printf("Enumerating PCI bus %02x...\n", bus);
#endif
    for (uint8_t device = 0; device < 32; device++) {
        pci_enumerate_device(bus_driver, bus, device);
    }
}
