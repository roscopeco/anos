/*
 * xHCI Driver Server
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "../common/device_types.h"
#include "include/xhci.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#ifdef DEBUG_XHCI_OPS
#define ops_debugf(...) printf(__VA_ARGS__)
#ifdef VERY_NOISY_XHCI_OPS
#define ops_vdebugf(...) printf(__VA_ARGS__)
#else
#define ops_vdebugf(...)
#endif
#else
#define ops_debugf(...)
#define ops_vdebugf(...)
#endif

static XHCIController controller = {0};
static XHCIPort ports[16] = {{0}}; // Support up to 16 ports
static uint64_t devman_channel = 0;
static uint64_t xhci_channel = 0;
static uint64_t pci_parent_id = 0;

static int xhci_initialize_driver(const uint64_t xhci_base,
                                  const uint64_t pci_config_base) {
#ifdef DEBUG_XHCI_INIT
    printf("Initializing xHCI driver:\n");
    printf("  xHCI Base: 0x%016lx\n", xhci_base);
    printf("  PCI Config Base: 0x%016lx\n", pci_config_base);
#endif

    if (!xhci_controller_init(&controller, xhci_base, pci_config_base)) {
        printf("Failed to initialize xHCI controller\n");
        return -1;
    }

#ifdef DEBUG_XHCI_INIT
    printf("xHCI controller initialized successfully\n");
    printf("  HCI Version: 0x%04x\n", controller.hci_version);
    printf("  Max Ports: %u\n", controller.max_ports);
    printf("  Max Slots: %u\n", controller.max_slots);
#endif

    // Start the controller
    if (!xhci_controller_start(&controller)) {
        printf("Failed to start xHCI controller\n");
        return -1;
    }

    // Initialize and scan all ports
    controller.active_ports = 0;
    for (uint8_t i = 0; i < controller.max_ports && i < 16; i++) {
        if (xhci_port_init(&ports[i], &controller, i)) {
            controller.active_ports |= (1U << i);
#ifdef DEBUG_XHCI_INIT
            printf("Port %u initialized - device connected\n", i);
#endif
        }
    }

#ifdef DEBUG_XHCI_INIT
    printf("Active ports: 0x%08x\n", controller.active_ports);
#endif

    return 0;
}

static bool register_with_devman(void) {
    const SyscallResult result = anos_find_named_channel("DEVMAN");
    if (result.result != SYSCALL_OK) {
        printf("Failed to find DEVMAN channel\n");
        return false;
    }

    devman_channel = result.value;

    // Create IPC channel for this driver
    const SyscallResult channel_result = anos_create_channel();
    if (channel_result.result != SYSCALL_OK) {
        printf("Failed to create xHCI driver IPC channel\n");
        return false;
    }

    xhci_channel = channel_result.value;
    static char __attribute__((aligned(4096))) reg_buffer[4096];

    // First, register the xHCI controller itself
    DeviceRegistrationMessage *reg_msg =
            (DeviceRegistrationMessage *)reg_buffer;
    reg_msg->msg_type = DEVICE_MSG_REGISTER;
    reg_msg->device_type = DEVICE_TYPE_USB;
    reg_msg->device_count = 1;

    DeviceInfo *controller_info = (DeviceInfo *)reg_msg->data;
    controller_info->device_id = 0; // Will be assigned by DEVMAN
    controller_info->parent_id = pci_parent_id;
    controller_info->device_type = DEVICE_TYPE_USB;
    controller_info->hardware_type = USB_HW_XHCI;
    controller_info->capabilities = DEVICE_CAP_HOTPLUG;
    controller_info->driver_channel = xhci_channel;

    snprintf(controller_info->name, sizeof(controller_info->name),
             "xHCI Controller");
    snprintf(controller_info->driver_name, sizeof(controller_info->driver_name),
             "xhcidrv");

    size_t msg_size = sizeof(DeviceRegistrationMessage) + sizeof(DeviceInfo);
    const SyscallResult controller_reg_result =
            anos_send_message(devman_channel, 0, msg_size, reg_buffer);

    uint64_t xhci_controller_id = 0;
    if (controller_reg_result.result == SYSCALL_OK &&
        controller_reg_result.value > 0) {
        xhci_controller_id = controller_reg_result.value;
        printf("Registered xHCI controller with DEVMAN (ID: %lu)\n",
               xhci_controller_id);
    } else {
        printf("Failed to register xHCI controller with DEVMAN\n");
        return false;
    }

    // Register each connected USB device
    for (uint8_t i = 0; i < controller.max_ports && i < 16; i++) {
        if ((controller.active_ports & (1U << i)) && ports[i].initialized) {
            reg_msg = (DeviceRegistrationMessage *)reg_buffer;
            reg_msg->msg_type = DEVICE_MSG_REGISTER;
            reg_msg->device_type = DEVICE_TYPE_USB;
            reg_msg->device_count = 1;

            UsbDeviceInfo *usb_info = (UsbDeviceInfo *)reg_msg->data;
            usb_info->base.device_id = 0; // Will be assigned by DEVMAN
            usb_info->base.parent_id = xhci_controller_id;
            usb_info->base.device_type = DEVICE_TYPE_USB;
            usb_info->base.hardware_type = USB_HW_XHCI;
            usb_info->base.capabilities = 0; // Will be set based on device type
            usb_info->base.driver_channel = xhci_channel;

            snprintf(usb_info->base.name, sizeof(usb_info->base.name),
                     "USB Device Port %u", i);
            snprintf(usb_info->base.driver_name,
                     sizeof(usb_info->base.driver_name), "xhcidrv");

            usb_info->vendor_id = ports[i].vendor_id;
            usb_info->product_id = ports[i].product_id;
            usb_info->device_class = ports[i].device_class;
            usb_info->device_subclass = ports[i].device_subclass;
            usb_info->device_protocol = ports[i].device_protocol;
            usb_info->port_number = i;
            usb_info->device_speed = ports[i].speed;

            strncpy(usb_info->manufacturer, ports[i].manufacturer,
                    sizeof(usb_info->manufacturer) - 1);
            strncpy(usb_info->product, ports[i].product,
                    sizeof(usb_info->product) - 1);
            strncpy(usb_info->serial_number, ports[i].serial_number,
                    sizeof(usb_info->serial_number) - 1);

            msg_size =
                    sizeof(DeviceRegistrationMessage) + sizeof(UsbDeviceInfo);
            const SyscallResult reg_result =
                    anos_send_message(devman_channel, 0, msg_size, reg_buffer);

            if (reg_result.result == SYSCALL_OK && reg_result.value > 0) {
                printf("Registered USB device on port %u with DEVMAN (ID: "
                       "%lu)\n",
                       i, reg_result.value);
            } else {
                printf("Failed to register USB device on port %u with DEVMAN\n",
                       i);
            }
        }
    }

    return true;
}

static void handle_usb_message(const uint64_t msg_cookie, const uint64_t tag,
                               void *buffer, const size_t buffer_size) {
    ops_vdebugf("xHCI: Received message with tag %lu, size %lu\n", tag,
                buffer_size);

    // For now, just acknowledge any messages
    // TODO: Implement proper USB device communication
    anos_reply_message(msg_cookie, 0);
}

int main(const int argc, char **argv) {
    printf("\nxHCI Driver #%s [libanos #%s]", VERSION, libanos_version());

    if (argc < 4) {
        printf("\n\nUsage: %s <xhci_base> <pci_config_base> <pci_parent_id>\n",
               argv[0]);
        printf("Arguments provided: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("  argv[%d]: %s\n", i, argv[i]);
        }
        return 1;
    }

    printf(" @ xHCI:0x%s PCI:0x%s Parent:%s\n", argv[1], argv[2], argv[3]);

    const uint64_t xhci_base = strtoull(argv[1], NULL, 16);
    const uint64_t pci_config_base = strtoull(argv[2], NULL, 16);
    pci_parent_id = strtoull(argv[3], NULL, 10);

    if (xhci_initialize_driver(xhci_base, pci_config_base) != 0) {
        printf("Failed to initialize xHCI driver\n");
        return 1;
    }

    ops_debugf("xHCI initialization @ 0x%s complete.\n", argv[1]);

    if (!register_with_devman()) {
        printf("Warning: Failed to register xHCI with DEVMAN\n");
    }

    ops_debugf("xHCI driver ready, entering message loop...\n");

    // Main message loop
    while (1) {
        void *ipc_buffer = (void *)0x300000000;
        size_t buffer_size = 4096;
        uint64_t tag = 0;

        const SyscallResult recv_result =
                anos_recv_message(xhci_channel, &tag, &buffer_size, ipc_buffer);
        const uint64_t msg_cookie = recv_result.value;

        if (recv_result.result == SYSCALL_OK && msg_cookie) {
            ops_vdebugf("xHCI: Received message with tag %lu, size %lu\n", tag,
                        buffer_size);
            handle_usb_message(msg_cookie, tag, ipc_buffer, buffer_size);
        } else {
            ops_debugf("xHCI: Error receiving message [0x%016lx]\n",
                       (uint64_t)recv_result.result);

            // Sleep briefly to avoid pegging the CPU if we're in an error loop
            anos_task_sleep_current_secs(1);
        }
    }
}