/*
 * stage3 - PCI enumeration routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_PCIDRV_PCI_ENUMERATE_H
#define __ANOS_PCIDRV_PCI_ENUMERATE_H

#include <stdbool.h>
#include <stdint.h>

#include "pci.h"

bool pci_device_exists(const PCIBusDriver *bus_driver, uint8_t bus,
                       uint8_t device, uint8_t function);

void pci_enumerate_function(const PCIBusDriver *bus_driver, uint8_t bus,
                            uint8_t device, uint8_t function);

void pci_enumerate_device(const PCIBusDriver *bus_driver, uint8_t bus,
                          uint8_t device);

void pci_enumerate_bus(const PCIBusDriver *bus_driver, uint8_t bus);

#endif //__ANOS_PCIDRV_PCI_ENUMERATE_H