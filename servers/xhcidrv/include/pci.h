/*
 * xhcidrv - PCI interface for xHCI driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * TODO this duplication is silly, should really be in /servers/common
 */

#ifndef __ANOS_XHCIDRV_PCI_H
#define __ANOS_XHCIDRV_PCI_H

#include <stdint.h>

// PCI Configuration Space offsets
#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_STATUS 0x06
#define PCI_CLASS_CODE 0x08
#define PCI_HEADER_TYPE 0x0E
#define PCI_BAR0 0x10
#define PCI_BAR1 0x14
#define PCI_SUBSYSTEM_ID 0x2E

// PCI Command register bits
#define PCI_CMD_IO_ENABLE (1 << 0)
#define PCI_CMD_MEM_ENABLE (1 << 1)
#define PCI_CMD_BUS_MASTER (1 << 2)
#define PCI_CMD_INTERRUPT_DISABLE (1 << 10)

uint32_t xhci_pci_read32(uint64_t config_base, uint8_t offset);
uint16_t xhci_pci_read16(uint64_t config_base, uint8_t offset);
uint8_t xhci_pci_read8(uint64_t config_base, uint8_t offset);
void xhci_pci_write32(uint64_t config_base, uint8_t offset, uint32_t value);
void xhci_pci_write16(uint64_t config_base, uint8_t offset, uint16_t value);

bool xhci_pci_enable_device(uint64_t config_base);

#endif