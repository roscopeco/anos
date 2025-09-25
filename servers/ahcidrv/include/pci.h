/*
 * ahcidrv - PCI for AHCI driver core definitions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_AHCIDRV_PCI_H
#define __ANOS_AHCIDRV_PCI_H

#include <stdbool.h>
#include <stdint.h>

// PCI Configuration Space offsets
#define PCI_CAPABILITY_LIST 0x34
#define PCI_CAP_ID_MSI 0x05

// MSI Capability structure offsets (from capability base)
#define MSI_CAP_CONTROL 0x02
#define MSI_CAP_ADDRESS_LO 0x04
#define MSI_CAP_ADDRESS_HI 0x08
#define MSI_CAP_DATA_32 0x08 // For 32-bit addressing
#define MSI_CAP_DATA_64 0x0C // For 64-bit addressing

// MSI Control register bits
#define MSI_CTRL_ENABLE (1 << 0)
#define MSI_CTRL_64BIT_CAPABLE (1 << 7)

uint32_t pci_read_config32(uint64_t pci_base, uint16_t offset);

uint16_t pci_read_config16(uint64_t pci_base, uint16_t offset);

uint8_t pci_read_config8(uint64_t pci_base, uint16_t offset);

void pci_write_config32(uint64_t pci_base, uint16_t offset, uint32_t value);

void pci_write_config16(uint64_t pci_base, uint16_t offset, uint16_t value);

uint8_t pci_find_msi_capability(uint64_t pci_base);

bool pci_configure_msi(uint64_t pci_base, uint8_t msi_offset, uint64_t msi_address, uint32_t msi_data);

#endif