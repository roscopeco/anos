/*
 * pcidrv - PCI low-level interface routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_PCIDRV_PCI_BUS_H
#define __ANOS_PCIDRV_PCI_BUS_H

#include <stddef.h>
#include <stdint.h>

#define PCI_REG_HIGH_W pci_reg_get_high_word
#define PCI_REG_LOW_W pci_reg_get_low_word
#define PCI_REG_UU_B pci_reg_get_upper_upper_byte
#define PCI_REG_UM_B pci_reg_get_upper_middle_byte
#define PCI_REG_LM_B pci_reg_get_lower_middle_byte
#define PCI_REG_LL_B pci_reg_get_lower_lower_byte

// PCI Configuration Space offsets
#define PCI_VENDOR_ID 0x00
#define PCI_DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define PCI_STATUS 0x06
#define PCI_CLASS_CODE 0x08
#define PCI_HEADER_TYPE 0x0E
#define PCI_SUBSYSTEM_ID 0x2E

// PCI Header Types
#define PCI_HEADER_TYPE_NORMAL 0x00
#define PCI_HEADER_TYPE_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS 0x02

// PCI Configuration Space layout
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
    uint32_t bar[6];
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base;
    uint8_t capabilities_pointer;
    uint8_t reserved[7];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} __attribute__((packed)) PCIConfigSpace;

typedef struct {
    uint64_t ecam_base;
    uint16_t segment;
    uint8_t bus_start;
    uint8_t bus_end;
    void *mapped_ecam;
    size_t mapped_size;
} PCIBusDriver;

uint32_t pci_config_read32(const PCIBusDriver *bus_driver, uint8_t bus,
                           uint8_t device, uint8_t function, uint8_t offset);

uint16_t pci_config_read16(const PCIBusDriver *bus_driver, uint8_t bus,
                           uint8_t device, uint8_t function, uint8_t offset);

uint8_t pci_config_read8(const PCIBusDriver *bus_driver, uint8_t bus,
                         uint8_t device, uint8_t function, uint8_t offset);

/* Register part bitmaps */
static inline uint16_t pci_reg_get_high_word(const uint32_t value) {
    return ((((value) & 0xFFFF0000) >> 16));
}

static inline uint16_t pci_reg_get_low_word(const uint32_t value) {
    return (((value) & 0x0000FFFF));
}

static inline uint8_t pci_reg_get_upper_upper_byte(const uint32_t value) {
    return ((((value) & 0xFF000000) >> 24));
}

static inline uint8_t pci_reg_get_upper_middle_byte(const uint32_t value) {
    return ((((value) & 0x00FF0000) >> 16));
}

static inline uint8_t pci_reg_get_lower_middle_byte(const uint32_t value) {
    return ((((value) & 0x0000FF00) >> 8));
}

static inline uint8_t pci_reg_get_lower_lower_byte(const uint32_t value) {
    return (((value) & 0x000000FF));
}

#endif
