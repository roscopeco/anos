/*
 * xHCI Controller Implementation
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anos/syscalls.h>

#include "include/pci.h"
#include "include/xhci.h"

#ifdef DEBUG_XHCI_INIT
#define init_debugf(...) printf(__VA_ARGS__)
#ifdef VERY_NOISY_XHCI_INIT
#define init_vdebugf(...) printf(__VA_ARGS__)
#else
#define init_vdebugf(...)
#endif
#else
#define init_debugf(...)
#define init_vdebugf(...)
#endif

#define PCI_CONFIG_BASE_ADDRESS 0xC000000000ULL
#define XHCI_VIRTUAL_BASE 0x400000000

uint32_t xhci_read32(const XHCIController *controller, volatile void *base,
                     const uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)((uint8_t *)base + offset);
    return *reg;
}

void xhci_write32(const XHCIController *controller, volatile void *base,
                  const uint32_t offset, const uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)((uint8_t *)base + offset);
    *reg = value;
}

bool xhci_controller_init(XHCIController *controller, const uint64_t base_addr,
                          const uint64_t pci_config_base) {
    init_debugf("Initializing xHCI controller at 0x%016lx\n", base_addr);

    memset(controller, 0, sizeof(XHCIController));
    controller->base_addr = base_addr;
    controller->pci_config_base = pci_config_base;

    // Map PCI configuration space first
    init_debugf("Mapping PCI config space: phys=0x%016lx -> virt=0x%016llx "
                "(size=0x1000)\n",
                pci_config_base, PCI_CONFIG_BASE_ADDRESS);

    const SyscallResult pci_result = anos_map_physical(
            pci_config_base, (void *)PCI_CONFIG_BASE_ADDRESS, 0x1000,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (pci_result.result != SYSCALL_OK) {
        printf("Failed to map PCI config space! Error code: %ld\n",
               pci_result.result);
        return false;
    }

    // Update PCI access to use mapped virtual address
    const uint64_t mapped_pci_config = PCI_CONFIG_BASE_ADDRESS;

    if (!xhci_pci_enable_device(mapped_pci_config)) {
        printf("Failed to enable xHCI PCI device\n");
        return false;
    }

    // Get BAR0 to determine register space size (for future use)
    init_vdebugf("BAR0: 0x%08x\n",
                 xhci_pci_read32(mapped_pci_config, PCI_BAR0));

    // For now, map a standard amount (64KB should be sufficient for most xHCI controllers)
    const size_t map_size = 64 * 1024;

    init_debugf("Mapping %zu bytes of xHCI register space...\n", map_size);

    const SyscallResult map_result = anos_map_physical(
            base_addr, (void *)XHCI_VIRTUAL_BASE, map_size,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (map_result.result != SYSCALL_OK) {
        printf("Failed to map xHCI register space! Error: %ld\n",
               map_result.result);
        return false;
    }

    controller->cap_regs = (volatile void *)XHCI_VIRTUAL_BASE;
    init_debugf("xHCI registers mapped at virtual address 0x%lx\n",
                (uintptr_t)controller->cap_regs);

    // Read capability registers
    const uint32_t cap_length_version =
            xhci_read32(controller, controller->cap_regs, XHCI_CAP_CAPLENGTH);
    controller->cap_length = cap_length_version & 0xFF;
    controller->hci_version = (cap_length_version >> 16) & 0xFFFF;

    init_debugf("Capability register length: %u bytes\n",
                controller->cap_length);
    init_debugf("HCI version: 0x%04x\n", controller->hci_version);

    // Set up operational register base
    controller->op_regs = (volatile void *)((uint8_t *)controller->cap_regs +
                                            controller->cap_length);

    // Read structural parameters
    const uint32_t hcsparams1 =
            xhci_read32(controller, controller->cap_regs, XHCI_CAP_HCSPARAMS1);
    controller->max_slots = hcsparams1 & 0xFF;
    controller->max_interrupters = (hcsparams1 >> 8) & 0x7FF;
    controller->max_ports = (hcsparams1 >> 24) & 0xFF;

    init_debugf("Maximum device slots: %u\n", controller->max_slots);
    init_debugf("Maximum interrupters: %u\n", controller->max_interrupters);
    init_debugf("Maximum ports: %u\n", controller->max_ports);

    // Set up port register base
    controller->port_regs =
            (volatile void *)((uint8_t *)controller->op_regs + 0x400);

    // Read doorbell and runtime register offsets
    const uint32_t dboff =
            xhci_read32(controller, controller->cap_regs, XHCI_CAP_DBOFF);
    const uint32_t rtsoff =
            xhci_read32(controller, controller->cap_regs, XHCI_CAP_RTSOFF);

    controller->doorbell_regs =
            (volatile void *)((uint8_t *)controller->cap_regs + (dboff & ~0x3));
    controller->runtime_regs =
            (volatile void *)((uint8_t *)controller->cap_regs +
                              (rtsoff & ~0x1F));

    init_vdebugf("Doorbell registers at offset 0x%x\n", dboff);
    init_vdebugf("Runtime registers at offset 0x%x\n", rtsoff);

    // Check if controller is already running
    const uint32_t usbsts =
            xhci_read32(controller, controller->op_regs, XHCI_OP_USBSTS);
    if (!(usbsts & XHCI_STS_HCH)) {
        init_debugf("Controller is running, stopping it first...\n");
        if (!xhci_controller_stop(controller)) {
            printf("Failed to stop running xHCI controller\n");
            return false;
        }
    }

    // Reset the controller
    if (!xhci_controller_reset(controller)) {
        printf("Failed to reset xHCI controller\n");
        return false;
    }

    controller->initialized = true;
    init_debugf("xHCI controller initialization complete\n");
    return true;
}

bool xhci_controller_reset(XHCIController *controller) {
    init_debugf("Resetting xHCI controller...\n");

    // Set the reset bit
    xhci_write32(controller, controller->op_regs, XHCI_OP_USBCMD,
                 XHCI_CMD_RESET);

    // Wait for reset to complete (CNR bit cleared)
    for (int timeout = 0; timeout < 100; timeout++) {
        const uint32_t usbsts =
                xhci_read32(controller, controller->op_regs, XHCI_OP_USBSTS);
        if (!(usbsts & XHCI_STS_CNR)) {
            init_debugf("xHCI controller reset complete\n");
            return true;
        }
        anos_task_sleep_current_secs(1);
    }

    printf("xHCI controller reset timeout\n");
    return false;
}

bool xhci_controller_start(XHCIController *controller) {
    init_debugf("Starting xHCI controller...\n");

    // Set the run bit
    uint32_t usbcmd =
            xhci_read32(controller, controller->op_regs, XHCI_OP_USBCMD);
    usbcmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    xhci_write32(controller, controller->op_regs, XHCI_OP_USBCMD, usbcmd);

    // Wait for controller to start (HCH bit cleared)
    for (int timeout = 0; timeout < 100; timeout++) {
        const uint32_t usbsts =
                xhci_read32(controller, controller->op_regs, XHCI_OP_USBSTS);
        if (!(usbsts & XHCI_STS_HCH)) {
            init_debugf("xHCI controller started successfully\n");
            return true;
        }
        anos_task_sleep_current_secs(1);
    }

    printf("xHCI controller start timeout\n");
    return false;
}

bool xhci_controller_stop(XHCIController *controller) {
    init_debugf("Stopping xHCI controller...\n");

    // Clear the run bit
    uint32_t usbcmd =
            xhci_read32(controller, controller->op_regs, XHCI_OP_USBCMD);
    usbcmd &= ~XHCI_CMD_RUN;
    xhci_write32(controller, controller->op_regs, XHCI_OP_USBCMD, usbcmd);

    // Wait for controller to stop (HCH bit set)
    for (int timeout = 0; timeout < 100; timeout++) {
        const uint32_t usbsts =
                xhci_read32(controller, controller->op_regs, XHCI_OP_USBSTS);
        if (usbsts & XHCI_STS_HCH) {
            init_debugf("xHCI controller stopped\n");
            return true;
        }
        anos_task_sleep_current_secs(1);
    }

    printf("xHCI controller stop timeout\n");
    return false;
}

bool xhci_port_init(XHCIPort *port, XHCIController *controller,
                    const uint8_t port_num) {
    init_vdebugf("Initializing xHCI port %u\n", port_num);

    if (port_num >= controller->max_ports) {
        printf("Invalid port number %u (max: %u)\n", port_num,
               controller->max_ports);
        return false;
    }

    memset(port, 0, sizeof(XHCIPort));
    port->controller = controller;
    port->port_num = port_num;

    return xhci_port_scan(port);
}

bool xhci_port_scan(XHCIPort *port) {
    const uint32_t port_offset = port->port_num * 0x10;
    const uint32_t portsc =
            xhci_read32(port->controller, port->controller->port_regs,
                        port_offset + XHCI_PORT_SC);

    port->connected = (portsc & XHCI_PORTSC_CCS) != 0;
    port->enabled = (portsc & XHCI_PORTSC_PED) != 0;

    if (port->connected) {
        port->speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> 10;
        init_vdebugf("Port %u: Device connected (speed: %u)\n", port->port_num,
                     port->speed);

        // For now, create generic device info
        port->vendor_id = 0x0000;  // Unknown
        port->product_id = 0x0000; // Unknown
        port->device_class = 0x09; // Hub class as default
        snprintf(port->manufacturer, sizeof(port->manufacturer), "Unknown");
        snprintf(port->product, sizeof(port->product), "USB Device on Port %u",
                 port->port_num);
        snprintf(port->serial_number, sizeof(port->serial_number), "PORT%u",
                 port->port_num);

        port->initialized = true;
        return true;
    } else {
        init_vdebugf("Port %u: No device connected\n", port->port_num);
        port->initialized = false;
        return false;
    }
}