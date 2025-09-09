/*
 * xhcidrv - PCI interface implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "include/pci.h"

#ifdef DEBUG_XHCI_PCI
#define pci_debugf(...) printf(__VA_ARGS__)
#else
#define pci_debugf(...)
#endif

// TODO this duplication is silly, should really be in /servers/common

uint32_t xhci_pci_read32(const uint64_t config_base, const uint8_t offset) {
    const volatile uint32_t *reg = (volatile uint32_t *)(config_base + offset);
    const uint32_t value = *reg;
    pci_debugf("PCI read32 [0x%lx + 0x%02x] = 0x%08x\n", config_base, offset,
               value);
    return value;
}

uint16_t xhci_pci_read16(const uint64_t config_base, const uint8_t offset) {
    const volatile uint16_t *reg = (volatile uint16_t *)(config_base + offset);
    const uint16_t value = *reg;
    pci_debugf("PCI read16 [0x%lx + 0x%02x] = 0x%04x\n", config_base, offset,
               value);
    return value;
}

uint8_t xhci_pci_read8(const uint64_t config_base, const uint8_t offset) {
    const volatile uint8_t *reg = (volatile uint8_t *)(config_base + offset);
    const uint8_t value = *reg;
    pci_debugf("PCI read8 [0x%lx + 0x%02x] = 0x%02x\n", config_base, offset,
               value);
    return value;
}

void xhci_pci_write32(const uint64_t config_base, const uint8_t offset,
                      const uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(config_base + offset);
    pci_debugf("PCI write32 [0x%lx + 0x%02x] = 0x%08x\n", config_base, offset,
               value);
    *reg = value;
}

void xhci_pci_write16(const uint64_t config_base, const uint8_t offset,
                      const uint16_t value) {
    volatile uint16_t *reg = (volatile uint16_t *)(config_base + offset);
    pci_debugf("PCI write16 [0x%lx + 0x%02x] = 0x%04x\n", config_base, offset,
               value);
    *reg = value;
}

bool xhci_pci_enable_device(const uint64_t config_base) {
    pci_debugf("Enabling xHCI PCI device at 0x%lx\n", config_base);

    uint16_t command = xhci_pci_read16(config_base, PCI_COMMAND);
    pci_debugf("Current PCI command register: 0x%04x\n", command);

    command |= PCI_CMD_MEM_ENABLE | PCI_CMD_BUS_MASTER;
    command &= ~PCI_CMD_INTERRUPT_DISABLE;

    xhci_pci_write16(config_base, PCI_COMMAND, command);

    const uint16_t new_command = xhci_pci_read16(config_base, PCI_COMMAND);
    if ((new_command & (PCI_CMD_MEM_ENABLE | PCI_CMD_BUS_MASTER)) !=
        (PCI_CMD_MEM_ENABLE | PCI_CMD_BUS_MASTER)) {
        printf("ERROR: Failed to enable xHCI PCI device\n");
        return false;
    }

    pci_debugf("xHCI PCI device enabled successfully (command: 0x%04x)\n",
               new_command);
    return true;
}