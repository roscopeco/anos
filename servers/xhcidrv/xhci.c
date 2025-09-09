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

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support nullptr or constexpr yet - Sept 2025
#ifndef nullptr
#ifdef NULL
#define nullptr NULL
#else
#define nullptr (((void *)0))
#endif
#endif
#define constexpr const
#endif

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

    // TODO tidy this up...

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
    constexpr uint64_t mapped_pci_config = PCI_CONFIG_BASE_ADDRESS;

    if (!xhci_pci_enable_device(mapped_pci_config)) {
        printf("Failed to enable xHCI PCI device\n");
        return false;
    }

    // Get BAR0 to determine register space size (for future use)
    init_vdebugf("BAR0: 0x%08x\n",
                 xhci_pci_read32(mapped_pci_config, PCI_BAR0));

    // For now, map a standard amount (64KB should be sufficient for most xHCI controllers)
    constexpr size_t map_size = 64 * 1024;

    init_debugf("Mapping %lu bytes of xHCI register space...\n", map_size);

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

    init_debugf("xHCI: Doorbell registers at offset 0x%x (virtual addr %p)\n",
                dboff, controller->doorbell_regs);
    init_debugf("xHCI: Runtime registers at offset 0x%x (virtual addr %p)\n",
                rtsoff, controller->runtime_regs);

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

bool xhci_controller_reset(const XHCIController *controller) {
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

        // TODO these feel a bit ... long.
        anos_task_sleep_current_secs(1);
    }

    printf("xHCI controller reset timeout\n");
    return false;
}

bool xhci_controller_start(const XHCIController *controller) {
    init_debugf("Starting xHCI controller...\n");

    // Set the run bit
    uint32_t usbcmd =
            xhci_read32(controller, controller->op_regs, XHCI_OP_USBCMD);
    usbcmd |= XHCI_CMD_RUN; // Remove XHCI_CMD_INTE for polling mode
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

bool xhci_controller_stop(const XHCIController *controller) {
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

#ifdef DEBUG_XHCI_INIT
#ifdef VERY_NOISY_XHCI_INIT
    const void *reg_addr = (const uint8_t *)port->controller->port_regs +
                           port_offset + XHCI_PORT_SC;

    init_vdebugf("xHCI: Port %u: offset=0x%x, reg_addr=%p (base=%p)\n",
                 port->port_num, port_offset, reg_addr,
                 port->controller->port_regs);
#endif
#endif

    uint32_t portsc = xhci_read32(port->controller, port->controller->port_regs,
                                  port_offset + XHCI_PORT_SC);

    port->connected = (portsc & XHCI_PORTSC_CCS) != 0;
    port->enabled = (portsc & XHCI_PORTSC_PED) != 0;

    init_debugf("xHCI: Scanning port %u: PORTSC=0x%08x, connected=%s\n",
                port->port_num, portsc,
                (portsc & XHCI_PORTSC_CCS) ? "YES" : "NO");

    if (port->connected) {
        init_debugf("xHCI: Port %u device detected, initiating reset\n",
                    port->port_num);

        // Initiate port reset by setting PR bit (bit 4)
        const uint32_t reset_portsc = portsc | XHCI_PORTSC_PR;
        xhci_write32(port->controller, port->controller->port_regs,
                     port_offset + XHCI_PORT_SC, reset_portsc);

        // Wait for reset to complete (PR bit clears automatically)
        init_debugf("xHCI: Waiting for port %u reset to complete\n",
                    port->port_num);
        int reset_timeout = 1000; // 1 second timeout
        while (reset_timeout-- > 0) {
            portsc = xhci_read32(port->controller, port->controller->port_regs,
                                 port_offset + XHCI_PORT_SC);

            if ((portsc & XHCI_PORTSC_PR) == 0) {
#ifdef DEBUG_XHCI_INIT
                uint8_t pls = (portsc & XHCI_PORTSC_PLS_MASK) >> 5;
                init_debugf("xHCI: Port %u reset completed, PORTSC=0x%08x, "
                            "PLS=%u\n",
                            port->port_num, portsc, pls);
#endif
                break;
            }

            anos_task_sleep_current(1000000);
        }

        if (reset_timeout <= 0) {
            printf("xHCI: Port %u reset timeout\n", port->port_num);
            return false;
        }

        // Re-read port status after reset
        portsc = xhci_read32(port->controller, port->controller->port_regs,
                             port_offset + XHCI_PORT_SC);
        port->connected = (portsc & XHCI_PORTSC_CCS) != 0;
        port->enabled = (portsc & XHCI_PORTSC_PED) != 0;

        if (!port->connected) {
            printf("xHCI: Port %u device disconnected after reset\n",
                   port->port_num);
            return false;
        }

        port->speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> 10;

        const char *speed_str;
        switch (port->speed) {
        case XHCI_SPEED_FULL:
            speed_str = "Full Speed (12 Mbps)";
            break;
        case XHCI_SPEED_LOW:
            speed_str = "Low Speed (1.5 Mbps)";
            break;
        case XHCI_SPEED_HIGH:
            speed_str = "High Speed (480 Mbps)";
            break;
        case XHCI_SPEED_SUPER:
            speed_str = "SuperSpeed (5 Gbps)";
            break;
        default:
            speed_str = "Unknown Speed";
            break;
        }

        const bool port_power = (portsc & XHCI_PORTSC_PP) != 0;
        const uint8_t link_state = (portsc & XHCI_PORTSC_PLS_MASK) >> 5;

        printf("xHCI Port %u: Device connected - %s, Power=%s, LinkState=%u, "
               "Enabled=%s\n",
               port->port_num, speed_str, port_power ? "On" : "Off", link_state,
               port->enabled ? "Yes" : "No");

        // Setup default port info, will populate more later when we enumerate.
        // defaulting here just so it's sane if enumeration fails...
        //
        port->vendor_id = 0x0000; // Populated later...
        port->product_id = 0x0000;

        port->device_class = 0;

        snprintf(port->manufacturer, sizeof(port->manufacturer),
                 "Unknown Manufacturer");
        snprintf(port->product, sizeof(port->product), "%s Device (Port %u)",
                 speed_str, port->port_num);
        snprintf(port->serial_number, sizeof(port->serial_number),
                 "XHCI-P%u-S%u", port->port_num, port->speed);

        port->initialized = true;

        return true;
    } else {
        init_vdebugf("Port %u: No device connected\n", port->port_num);
        port->initialized = false;
        return false;
    }
}