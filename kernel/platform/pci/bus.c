/*
 * stage3 - PCI low-level interface routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "platform/pci/bus.h"
#include "machine.h"
#include <stdint.h>

#define PCI_ADDRESS_ENABLE_MASK 0x80000000

#define PCI_CONFIG_ADDRESS_PORT 0xcf8
#define PCI_CONFIG_DATA_PORT 0xcfc

#define PCI_DEVICE_MAX_MASK ((PCI_MAX_DEVICE_COUNT - 1))
#define PCI_FUNC_MAX_MASK ((PCI_MAX_FUNC_COUNT - 1))
#define PCI_REG_MAX_MASK ((PCI_MAX_REG_COUNT - 1))

uint32_t pci_address_reg(uint8_t bus, uint8_t device, uint8_t func,
                         uint8_t reg) {
    return PCI_ADDRESS_ENABLE_MASK | ((reg & PCI_REG_MAX_MASK) << 2) |
           ((func & PCI_FUNC_MAX_MASK) << 8) |
           ((device & PCI_DEVICE_MAX_MASK) << 11) | (bus << 16);
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t func,
                               uint8_t reg) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address_reg(bus, device, func, reg));
    return inl(PCI_CONFIG_DATA_PORT);
}