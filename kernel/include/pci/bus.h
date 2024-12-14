/*
 * stage3 - PCI low-level interface routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_KERNEL_PCI_BUS_H
#define __ANOS_KERNEL_PCI_BUS_H

#include <stdint.h>

#define PCI_MAX_BUS_COUNT ((256))
#define PCI_MAX_DEVICE_COUNT ((32))
#define PCI_MAX_FUNC_COUNT ((8))
#define PCI_MAX_REG_COUNT ((64))

#define PCI_REG_COMMON_IDENT ((0))
#define PCI_REG_COMMON_CMD_STATUS ((1))
#define PCI_REG_COMMON_CLASS ((2))
#define PCI_REG_COMMON_BIST_TYPE ((3))

#define PCI_HEADER_TYPE(header_type) ((header_type & 0x7f))
#define PCI_HEADER_MULTIFUNCTION(header_type) ((header_type & 0x80))

#define PCI_REG_BRIDGE_BUSN ((6))

#define PCI_ADDR_ENABLE pci_addr_get_enable
#define PCI_ADDR_BUS pci_addr_get_bus
#define PCI_ADDR_DEVICE pci_addr_get_device
#define PCI_ADDR_FUNC pci_addr_get_func
#define PCI_ADDR_REG pci_addr_get_reg

#define PCI_REG_HIGH_W pci_reg_get_high_word
#define PCI_REG_LOW_W pci_reg_get_low_word
#define PCI_REG_UU_B pci_reg_get_upper_upper_byte
#define PCI_REG_UM_B pci_reg_get_upper_middle_byte
#define PCI_REG_LM_B pci_reg_get_lower_middle_byte
#define PCI_REG_LL_B pci_reg_get_lower_lower_byte

/* Address register bitmasks */
static inline uint8_t pci_addr_get_enable(uint32_t address) {
    return ((((address) & 0x80000000) >> 31));
}

static inline uint8_t pci_addr_get_bus(uint32_t address) {
    return ((((address) & 0x00FF0000) >> 16));
}

static inline uint8_t pci_addr_get_device(uint32_t address) {
    return ((((address) & 0x0000F800) >> 11));
}

static inline uint8_t pci_addr_get_func(uint32_t address) {
    return ((((address) & 0x00000700) >> 8));
}

static inline uint8_t pci_addr_get_reg(uint32_t address) {
    return ((((address) & 0x000000FC) >> 2));
}

/* Register part bitmaps */
static inline uint16_t pci_reg_get_high_word(uint32_t value) {
    return ((((value) & 0xFFFF0000) >> 16));
}

static inline uint16_t pci_reg_get_low_word(uint32_t value) {
    return (((value) & 0x0000FFFF));
}

static inline uint8_t pci_reg_get_upper_upper_byte(uint32_t value) {
    return ((((value) & 0xFF000000) >> 24));
}

static inline uint8_t pci_reg_get_upper_middle_byte(uint32_t value) {
    return ((((value) & 0x00FF0000) >> 16));
}

static inline uint8_t pci_reg_get_lower_middle_byte(uint32_t value) {
    return ((((value) & 0x0000FF00) >> 8));
}

static inline uint8_t pci_reg_get_lower_lower_byte(uint32_t value) {
    return (((value) & 0x000000FF));
}

uint32_t pci_address_reg(uint8_t bus, uint8_t device, uint8_t func,
                         uint8_t reg);

uint32_t pci_config_read_dword(uint8_t bus, uint8_t device, uint8_t func,
                               uint8_t reg);

#endif //__ANOS_KERNEL_PCI_BUS_H