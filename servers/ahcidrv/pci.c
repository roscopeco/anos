/*
 * ahcidrv - PCI routines for AHCI driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "pci.h"

#ifdef DEBUG_AHCI_PCI
#include <stdio.h>
#define debugf(...) printf(__VA_ARGS__)
#else
#define debugf(...)
#endif

uint32_t pci_read_config32(const uint64_t pci_base, const uint16_t offset) {
    volatile uint32_t *config_space = (volatile uint32_t *)pci_base;
    return config_space[offset / 4];
}

uint16_t pci_read_config16(const uint64_t pci_base, const uint16_t offset) {
    const uint32_t dword = pci_read_config32(pci_base, offset & ~3);
    return (dword >> ((offset & 3) * 8)) & 0xFFFF;
}

uint8_t pci_read_config8(const uint64_t pci_base, const uint16_t offset) {
    const uint32_t dword = pci_read_config32(pci_base, offset & ~3);
    return (dword >> ((offset & 3) * 8)) & 0xFF;
}

void pci_write_config32(const uint64_t pci_base, const uint16_t offset,
                        const uint32_t value) {
    volatile uint32_t *config_space = (volatile uint32_t *)pci_base;
    config_space[offset / 4] = value;
}

void pci_write_config16(const uint64_t pci_base, const uint16_t offset,
                        const uint16_t value) {
    uint32_t dword = pci_read_config32(pci_base, offset & ~3);
    const uint32_t shift = (offset & 3) * 8;
    dword = (dword & ~(0xFFFFU << shift)) | ((uint32_t)value << shift);
    pci_write_config32(pci_base, offset & ~3, dword);
}

uint8_t pci_find_msi_capability(const uint64_t pci_base) {
    // Check if capabilities are supported
    const volatile uint16_t status = pci_read_config16(pci_base, 0x06);
    if (!(status & (1 << 4))) {
        debugf("PCI device does not support capabilities\n");
        return 0;
    }

    uint8_t cap_ptr = pci_read_config8(pci_base, PCI_CAPABILITY_LIST) & 0xFC;

    while (cap_ptr != 0) {
        const uint8_t cap_id = pci_read_config8(pci_base, cap_ptr);
        if (cap_id == PCI_CAP_ID_MSI) {
            debugf("Found MSI capability at offset 0x%02x\n", cap_ptr);
            return cap_ptr;
        }
        cap_ptr = pci_read_config8(pci_base, cap_ptr + 1) & 0xFC;
    }

    debugf("MSI capability not found\n");
    return 0;
}

bool pci_configure_msi(const uint64_t pci_base, uint8_t msi_offset,
                       uint64_t msi_address, uint32_t msi_data) {
    if (msi_offset == 0) {
        return false;
    }

    debugf("Configuring MSI at offset 0x%02x: addr=0x%016lx, data=0x%08x\n",
           msi_offset, msi_address, msi_data);

    // Read MSI control register
    uint16_t msi_control =
            pci_read_config16(pci_base, msi_offset + MSI_CAP_CONTROL);
    const bool is_64bit = (msi_control & MSI_CTRL_64BIT_CAPABLE) != 0;

    debugf("MSI capability: %s addressing\n", is_64bit ? "64-bit" : "32-bit");

    // Disable MSI first
    msi_control &= ~MSI_CTRL_ENABLE;
    pci_write_config16(pci_base, msi_offset + MSI_CAP_CONTROL, msi_control);

    // Write MSI address
    pci_write_config32(pci_base, msi_offset + MSI_CAP_ADDRESS_LO,
                       (uint32_t)(msi_address & 0xFFFFFFFF));

    if (is_64bit) {
        pci_write_config32(pci_base, msi_offset + MSI_CAP_ADDRESS_HI,
                           (uint32_t)(msi_address >> 32));
        pci_write_config32(pci_base, msi_offset + MSI_CAP_DATA_64, msi_data);
    } else {
        pci_write_config32(pci_base, msi_offset + MSI_CAP_DATA_32, msi_data);
    }

    // Enable MSI
    msi_control |= MSI_CTRL_ENABLE;
    pci_write_config16(pci_base, msi_offset + MSI_CAP_CONTROL, msi_control);

    debugf("MSI enabled successfully\n");
    return true;
}
