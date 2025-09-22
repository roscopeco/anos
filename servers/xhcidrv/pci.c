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

uint8_t xhci_pci_find_msi_capability(const uint64_t config_base) {
    // Check if capabilities are supported
    const uint16_t status = xhci_pci_read16(config_base, 0x06);
    if (!(status & (1U << 4))) {
        pci_debugf("PCI device does not support capabilities\n");
        return 0;
    }

    uint8_t cap_ptr = xhci_pci_read8(config_base, PCI_CAPABILITY_LIST) & 0xFC;

    while (cap_ptr != 0) {
        const uint8_t cap_id = xhci_pci_read8(config_base, cap_ptr);
        if (cap_id == PCI_CAP_ID_MSI) {
            pci_debugf("Found MSI capability at offset 0x%02x\n", cap_ptr);
            return cap_ptr;
        }
        if (cap_id == PCI_CAP_ID_MSIX) {
            pci_debugf("Found MSI-X capability at offset 0x%02x\n", cap_ptr);
            return cap_ptr;
        }
        cap_ptr = xhci_pci_read8(config_base, cap_ptr + 1) & 0xFC;
    }

    pci_debugf("MSI or MSI-X capability not found\n");
    return 0;
}

bool xhci_pci_configure_msi(const uint64_t config_base,
                            const uint8_t msi_offset,
                            const uint64_t msi_address,
                            const uint32_t msi_data) {
    if (msi_offset == 0) {
        pci_debugf("No MSI offset detected; MSI not configured.\n");
        return false;
    }

    // Check capability type to determine MSI vs MSI-X
    const uint8_t cap_id = xhci_pci_read8(config_base, msi_offset);
    pci_debugf("Capability ID at offset 0x%02x: 0x%02x\n", msi_offset, cap_id);
    pci_debugf("Configuring %s at offset 0x%02x: addr=0x%016lx, data=0x%08x\n",
               (cap_id == PCI_CAP_ID_MSI) ? "MSI" : "MSI-X", msi_offset,
               msi_address, msi_data);

    if (cap_id == PCI_CAP_ID_MSIX) {
        pci_debugf("Detected MSI-X (0x%02x), configuring MSI-X interrupts\n",
                   PCI_CAP_ID_MSIX);
        // We need the xHCI base address to configure MSI-X table, but we don't have it here
        // For now, just enable MSI-X without table configuration
        pci_debugf("MSI-X table configuration not implemented in generic "
                   "function\n");
        return false;
    }

    pci_debugf("Continuing with MSI configuration for capability ID 0x%02x\n",
               cap_id);

    // Read MSI control register
    uint16_t msi_control =
            xhci_pci_read16(config_base, msi_offset + MSI_CAP_CONTROL);
    const bool is_64bit = (msi_control & MSI_CTRL_64BIT_CAPABLE) != 0;

    pci_debugf("MSI capability: %s addressing\n",
               is_64bit ? "64-bit" : "32-bit");

    // Disable MSI first
    msi_control &= ~MSI_CTRL_ENABLE;
    xhci_pci_write16(config_base, msi_offset + MSI_CAP_CONTROL, msi_control);

    // Write MSI address
    xhci_pci_write32(config_base, msi_offset + MSI_CAP_ADDRESS_LO,
                     (uint32_t)(msi_address & 0xFFFFFFFF));

    if (is_64bit) {
        xhci_pci_write32(config_base, msi_offset + MSI_CAP_ADDRESS_HI,
                         (uint32_t)(msi_address >> 32));
        xhci_pci_write32(config_base, msi_offset + MSI_CAP_DATA_64, msi_data);
    } else {
        xhci_pci_write32(config_base, msi_offset + MSI_CAP_DATA_32, msi_data);
    }

    // Enable MSI
    msi_control |= MSI_CTRL_ENABLE;
    xhci_pci_write16(config_base, msi_offset + MSI_CAP_CONTROL, msi_control);

    pci_debugf("MSI enabled successfully\n");
    return true;
}

bool xhci_pci_configure_msix(const uint64_t config_base,
                             const uint8_t msix_offset,
                             const uint64_t msi_address,
                             const uint32_t msi_data,
                             const uint64_t xhci_base) {
    pci_debugf("Configuring MSI-X at offset 0x%02x\n", msix_offset);

    // Read MSI-X control register
    const uint16_t msix_control =
            xhci_pci_read16(config_base, msix_offset + MSIX_CAP_CONTROL);

#ifdef DEBUG_XHCI_PCI
    const uint16_t table_size =
            (msix_control & 0x07FF) + 1; // Bits 10:0 = table size - 1
    pci_debugf("MSI-X table size: %d entries\n", table_size);
#endif

    // Read table offset and BIR (BAR Indicator Register)
    const uint32_t table_offset_bir =
            xhci_pci_read32(config_base, msix_offset + MSIX_CAP_TABLE_OFFSET);
    const uint8_t table_bir = table_offset_bir & 0x07; // Bits 2:0 = BAR number
    const uint32_t table_offset =
            table_offset_bir & ~0x07; // Bits 31:3 = offset

    pci_debugf("MSI-X table: BAR%d + 0x%08x\n", table_bir, table_offset);

    // Determine MSI-X table address based on the BIR
    uint64_t msix_table_addr;
    if (table_bir == 0) {
        // Table is in BAR0 (xHCI register space)
        msix_table_addr = xhci_base + table_offset;
    } else {
        // Table is in a different BAR - would need to map that BAR
        pci_debugf("MSI-X table in BAR%d not currently supported\n", table_bir);
        return false;
    }

    // Configure MSI-X table entry 0
    // MSI-X table entry structure (16 bytes per entry):
    // Offset 0x00: Message Address Low (32-bit)
    // Offset 0x04: Message Address High (32-bit)
    // Offset 0x08: Message Data (32-bit)
    // Offset 0x0C: Vector Control (32-bit) - bit 0 = mask
    pci_debugf("MSI-X table at virtual address 0x%016lx\n", msix_table_addr);

    // Configure table entry 0 with the allocated interrupt vector
    volatile uint32_t *table_entry = (volatile uint32_t *)msix_table_addr;

    pci_debugf("Writing MSI-X table entry 0: addr=0x%016lx, data=0x%08x\n",
               msi_address, msi_data);

    // Write message address (split into low/high 32-bit parts)
    table_entry[0] = (uint32_t)(msi_address & 0xFFFFFFFF); // Address Low
    table_entry[1] = (uint32_t)(msi_address >> 32);        // Address High
    table_entry[2] = msi_data;                             // Message Data
    table_entry[3] = 0x00000000; // Vector Control (unmask interrupt)

    pci_debugf("MSI-X table entry 0 configured\n");

    // Enable MSI-X - this will enable interrupt generation
    const uint16_t new_control = msix_control | MSIX_CTRL_ENABLE;
    xhci_pci_write16(config_base, msix_offset + MSIX_CAP_CONTROL, new_control);

    pci_debugf("MSI-X enabled with table configuration complete\n");
    return true;
}