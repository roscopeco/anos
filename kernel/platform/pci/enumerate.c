/*
 * stage3 - PCI enumeration routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "platform/pci/bus.h"
#include "platform/pci/class.h"
#include "printhex.h"

static void pci_scan_bus(const uint8_t bus);

static uint32_t pci_bridge_bus_numbers(const uint8_t bus, const uint8_t device,
                                       const uint8_t func) {
    return pci_config_read_dword(bus, device, func, PCI_REG_BRIDGE_BUSN);
}

static void pci_enumerate_bridge(const uint8_t bus, const uint8_t device,
                                 const uint8_t func) {
    // pci-pci bridge
    uint32_t bus_numbers = pci_bridge_bus_numbers(bus, device, func);
    uint8_t secondary = PCI_REG_LM_B(bus_numbers);
    uint8_t subordinate = PCI_REG_UM_B(bus_numbers);

    for (int sub_bus = secondary; sub_bus <= subordinate; sub_bus++) {
        pci_scan_bus(sub_bus);
    }
}

static void pci_enumerate_device(const uint8_t bus, const uint8_t device,
                                 const uint8_t func) {
    uint32_t ident_d =
            pci_config_read_dword(bus, device, func, PCI_REG_COMMON_IDENT);
    uint16_t device_id = PCI_REG_HIGH_W(ident_d);
    uint16_t vendor_id = PCI_REG_LOW_W(ident_d);

    if (device_id != 0xffff && vendor_id != 0xffff) {
        uint32_t bist_d = pci_config_read_dword(bus, device, func,
                                                PCI_REG_COMMON_BIST_TYPE);
        uint8_t header_type = PCI_REG_UM_B(bist_d);

        uint32_t class_d =
                pci_config_read_dword(bus, device, func, PCI_REG_COMMON_CLASS);
        uint8_t class = PCI_REG_UU_B(class_d);
        uint8_t subclass = PCI_REG_UM_B(class_d);

#ifdef DEBUG_PCI_ENUM
        uint8_t prog_if = PCI_REG_LM_B(class_d);
        uint8_t revision_id = PCI_REG_LL_B(class_d);

        debugstr("PCI ");
        printhex8(bus, debugchar);
        debugstr(":");
        printhex8(device, debugchar);
        debugstr(":");
        printhex8(func, debugchar);
        debugstr(": [");
        printhex16(vendor_id, debugchar);
        debugstr(":");
        printhex16(device_id, debugchar);
        debugstr("] - ");
        printhex8(class, debugchar);
        debugstr(":");
        printhex8(subclass, debugchar);
        debugstr(":");
        printhex8(prog_if, debugchar);
        debugstr(":");
        printhex8(revision_id, debugchar);
        debugstr(" [type ");
        printhex8(header_type, debugchar);
        debugstr("]");
#ifdef VERY_NOISY_PCI_ENUM
        uint32_t status_d = pci_config_read_dword(bus, device, func,
                                                  PCI_REG_COMMON_CMD_STATUS);

        uint16_t status = PCI_REG_HIGH_W(status_d);
        uint16_t command = PCI_REG_LOW_W(status_d);

        uint8_t bist = PCI_REG_UU_B(bist_d);
        uint8_t latency_timer = PCI_REG_LM_B(bist_d);
        uint8_t cache_line_size = PCI_REG_LM_B(bist_d);

        debugstr("\n  +-- Command: ");
        printhex16(command, debugchar);
        debugstr("; Status ");
        printhex16(status, debugchar);
        debugstr("\n  +-- BIST: ");
        printhex8(bist, debugchar);
        debugstr("; Latency Timer: ");
        printhex8(latency_timer, debugchar);
        debugstr("; Cache line size: ");
        printhex8(cache_line_size, debugchar);
        debugstr("\n");
#else
        debugstr("\n");
#endif
#endif
        if (class == PCI_CLASS_BRIDGE &&
            subclass == PCI_CLASS_BRIDGE_SUBCLASS_PCI_PCI_4 &&
            PCI_HEADER_TYPE(header_type) == 0x01) {
            pci_enumerate_bridge(bus, device, func);
        }

        if (PCI_HEADER_MULTIFUNCTION(header_type) && func == 0) {
            // first device of a multifunction - scan others
            for (int next_func = 1; next_func < PCI_MAX_FUNC_COUNT;
                 next_func++) {
                pci_enumerate_device(bus, device, next_func);
            }
        }
    }
}

static void pci_scan_bus(const uint8_t bus) {
    for (int device = 0; device < PCI_MAX_DEVICE_COUNT; device++) {
        pci_enumerate_device(bus, device, 0);
    }
}

void pci_enumerate() {
    // Make sure we have PCI?

    pci_scan_bus(0);
}