/*
 * AHCI Driver Server
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
#include "ahci.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static AHCIController controller = {0};
static AHCIPort ports[32] = {nullptr};
static uint64_t devman_channel = 0;
static uint64_t pci_parent_id = 0;

static int ahci_initialize_driver(const uint64_t ahci_base,
                                  const uint64_t pci_config_base) {
#ifdef DEBUG_AHCI_INIT
    printf("Initializing AHCI driver:\n");
    printf("  AHCI Base: 0x%016lx\n", ahci_base);
    printf("  PCI Config Base: 0x%016lx\n", pci_config_base);
#endif

    if (!ahci_controller_init(&controller, ahci_base, pci_config_base)) {
        printf("Failed to initialize AHCI controller\n");
        return -1;
    }

#ifdef DEBUG_AHCI_INIT
    printf("AHCI controller initialized successfully\n");
    printf("  Port count: %u\n", controller.port_count);
    printf("  Active ports: 0x%08x\n", controller.active_ports);
#endif

    for (uint8_t i = 0; i < controller.port_count; i++) {
        if (controller.active_ports & (1 << i)) {
            if (ahci_port_init(&ports[i], &controller, i)) {
#ifdef DEBUG_AHCI_INIT
                printf("Port %u initialized successfully\n", i);
#endif
                if (ahci_port_identify(&ports[i])) {
#ifdef DEBUG_AHCI_INIT
                    printf("Port %u: Device identified - %lu sectors, %u "
                           "bytes/sector\n",
                           i, ports[i].sector_count, ports[i].sector_size);
#endif
                } else {
#ifdef DEBUG_AHCI_INIT
                    printf("Warning: Failed to identify device on port %u\n",
                           i);
#endif
                }
            } else {
#ifdef DEBUG_AHCI_INIT
                printf("Warning: Failed to initialize port %u\n", i);
#endif
            }
        }
    }

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
        printf("Failed to create AHCI driver IPC channel\n");
        return false;
    }

    const uint64_t ahci_channel = channel_result.value;
    static char __attribute__((aligned(4096))) reg_buffer[4096];

    // First, register the AHCI controller itself
    DeviceRegistrationMessage *reg_msg =
            (DeviceRegistrationMessage *)reg_buffer;
    reg_msg->msg_type = DEVICE_MSG_REGISTER;
    reg_msg->device_type = DEVICE_TYPE_STORAGE;
    reg_msg->device_count = 1;

    DeviceInfo *controller_info = (DeviceInfo *)reg_msg->data;
    controller_info->device_id = 0; // Will be assigned by DEVMAN
    controller_info->parent_id = pci_parent_id;
    controller_info->device_type = DEVICE_TYPE_STORAGE;
    controller_info->hardware_type = STORAGE_HW_AHCI;
    controller_info->capabilities = 0; // Controller itself doesn't do I/O
    controller_info->driver_channel = ahci_channel;

    snprintf(controller_info->name, sizeof(controller_info->name),
             "AHCI Controller");
    snprintf(controller_info->driver_name, sizeof(controller_info->driver_name),
             "ahcidrv");

    size_t msg_size = sizeof(DeviceRegistrationMessage) + sizeof(DeviceInfo);
    const SyscallResult controller_reg_result =
            anos_send_message(devman_channel, 0, msg_size, reg_buffer);

    uint64_t ahci_controller_id = 0;
    if (controller_reg_result.result == SYSCALL_OK &&
        controller_reg_result.value > 0) {
        ahci_controller_id = controller_reg_result.value;
        printf("Registered AHCI controller with DEVMAN (ID: %lu)\n",
               ahci_controller_id);
    } else {
        printf("Failed to register AHCI controller with DEVMAN\n");
        return false;
    }

    // Then register each active port as a storage device under the controller
    for (uint32_t i = 0; i < controller.port_count; i++) {
        if (controller.active_ports & (1 << i) && ports[i].initialized) {
            reg_msg = (DeviceRegistrationMessage *)reg_buffer;
            reg_msg->msg_type = DEVICE_MSG_REGISTER;
            reg_msg->device_type = DEVICE_TYPE_STORAGE;
            reg_msg->device_count = 1;

            StorageDeviceInfo *storage_info =
                    (StorageDeviceInfo *)reg_msg->data;
            storage_info->base.device_id = 0; // Will be assigned by DEVMAN
            storage_info->base.parent_id =
                    ahci_controller_id; // Parent is the AHCI controller
            storage_info->base.device_type = DEVICE_TYPE_STORAGE;
            storage_info->base.hardware_type = STORAGE_HW_AHCI;
            storage_info->base.capabilities =
                    DEVICE_CAP_READ | DEVICE_CAP_WRITE;
            storage_info->base.driver_channel = ahci_channel;

            snprintf(storage_info->base.name, sizeof(storage_info->base.name),
                     "AHCI Port %u", i);
            snprintf(storage_info->base.driver_name,
                     sizeof(storage_info->base.driver_name), "ahcidrv");

            storage_info->sector_count = ports[i].sector_count;
            storage_info->sector_size = ports[i].sector_size;

            // TODO: Extract model/serial from identify data
            snprintf(storage_info->model, sizeof(storage_info->model),
                     "Unknown Model");
            snprintf(storage_info->serial, sizeof(storage_info->serial),
                     "Unknown Serial");

            msg_size = sizeof(DeviceRegistrationMessage) +
                       sizeof(StorageDeviceInfo);

            const SyscallResult reg_result =
                    anos_send_message(devman_channel, 0, msg_size, reg_buffer);

            if (reg_result.result == SYSCALL_OK && reg_result.value > 0) {
                printf("Registered storage device on port %u with DEVMAN (ID: "
                       "%lu)\n",
                       i, reg_result.value);
            } else {
                printf("Failed to register port %u with DEVMAN\n", i);
            }
        }
    }

    return true;
}

int main(const int argc, char **argv) {
    printf("\nAHCI Driver #%s [libanos #%s]", VERSION, libanos_version());

    if (argc < 4) {
        printf("\n\nUsage: %s <ahci_base> <pci_config_base> <pci_parent_id>\n",
               argv[0]);
        printf("Arguments provided: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("  argv[%d]: %s\n", i, argv[i]);
        }
        return 1;
    }

    printf(" @ AHCI:0x%s PCI:0x%s Parent:%s\n", argv[1], argv[2], argv[3]);

    const uint64_t ahci_base = strtoull(argv[1], nullptr, 16);
    const uint64_t pci_config_base = strtoull(argv[2], nullptr, 16);
    pci_parent_id = strtoull(argv[3], nullptr, 10);

    if (ahci_initialize_driver(ahci_base, pci_config_base) != 0) {
        printf("Failed to initialize AHCI driver\n");
        return 1;
    }

    printf("AHCI initialization @ 0x%s complete.\n", argv[1]);

    // Register devices with DEVMAN
    if (!register_with_devman()) {
        printf("Warning: Failed to register with DEVMAN\n");
    }

    while (1) {
        anos_task_sleep_current_secs(5);
    }

    ahci_controller_cleanup(&controller);
    return 0;
}