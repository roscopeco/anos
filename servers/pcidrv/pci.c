/*
 * PCI low-level interface routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>

#include "pci.h"

uint32_t pci_config_read32(const PCIBusDriver *bus_driver, const uint8_t bus, const uint8_t device,
                           const uint8_t function, const uint8_t offset) {
    if (!bus_driver) {
        return 0xFFFFFFFF;
    }

#ifdef CONSERVATIVE_BUILD
    if (bus < bus_driver->bus_start || bus > bus_driver->bus_end) {
        return 0xFFFFFFFF;
    }

    if (!bus_driver->mapped_ecam) {
        return 0xFFFFFFFF;
    }
#endif

    const uint64_t device_offset = ((uint64_t)(bus - bus_driver->bus_start) << 20) | ((uint64_t)device << 15) |
                                   ((uint64_t)function << 12) | offset;

    const uint32_t *config_addr = (uint32_t *)((uint8_t *)bus_driver->mapped_ecam + device_offset);
    return *config_addr;
}

uint16_t pci_config_read16(const PCIBusDriver *bus_driver, const uint8_t bus, const uint8_t device,
                           const uint8_t function, const uint8_t offset) {
    const uint32_t dword = pci_config_read32(bus_driver, bus, device, function, offset & ~3);
    return (uint16_t)(dword >> ((offset & 3) * 8));
}

uint8_t pci_config_read8(const PCIBusDriver *bus_driver, const uint8_t bus, const uint8_t device,
                         const uint8_t function, const uint8_t offset) {
    const uint32_t dword = pci_config_read32(bus_driver, bus, device, function, offset & ~3);
    return (uint8_t)(dword >> ((offset & 3) * 8));
}
