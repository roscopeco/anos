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

// PCI Capability List
#define PCI_CAPABILITY_LIST 0x34
#define PCI_CAP_ID_MSI 0x05
#define PCI_CAP_ID_MSIX 0x11

// MSI Capability structure offsets (from capability base)
#define MSI_CAP_CONTROL 0x02
#define MSI_CAP_ADDRESS_LO 0x04
#define MSI_CAP_ADDRESS_HI 0x08
#define MSI_CAP_DATA_32 0x08 // For 32-bit addressing
#define MSI_CAP_DATA_64 0x0C // For 64-bit addressing

// MSI Control register bits
#define MSI_CTRL_ENABLE (1 << 0)
#define MSI_CTRL_64BIT_CAPABLE (1 << 7)

// MSI-X Capability structure offsets (from capability base)
#define MSIX_CAP_CONTROL 0x02
#define MSIX_CAP_TABLE_OFFSET 0x04
#define MSIX_CAP_PBA_OFFSET 0x08

// MSI-X Control register bits
#define MSIX_CTRL_ENABLE (1 << 15)
#define MSIX_CTRL_FUNCTION_MASK (1 << 14)

uint32_t xhci_pci_read32(uint64_t config_base, uint8_t offset);
uint16_t xhci_pci_read16(uint64_t config_base, uint8_t offset);
uint8_t xhci_pci_read8(uint64_t config_base, uint8_t offset);
void xhci_pci_write32(uint64_t config_base, uint8_t offset, uint32_t value);
void xhci_pci_write16(uint64_t config_base, uint8_t offset, uint16_t value);

bool xhci_pci_enable_device(uint64_t config_base);

uint8_t xhci_pci_find_msi_capability(uint64_t config_base);
bool xhci_pci_configure_msi(uint64_t config_base, uint8_t msi_offset,
                            uint64_t msi_address, uint32_t msi_data);
bool xhci_pci_configure_msix(uint64_t config_base, uint8_t msix_offset,
                             uint64_t msi_address, uint32_t msi_data,
                             uint64_t xhci_base);

#endif