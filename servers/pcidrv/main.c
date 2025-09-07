/*
 * PCI Bus Driver Server
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
#include "enumerate.h"
#include "pci.h"

#define PROCESS_SPAWN ((1))

typedef struct {
    uint64_t capability_id;
    uint64_t capability_cookie;
} InitCapability;

typedef struct {
    uint64_t stack_size;
    uint16_t argc;
    uint16_t capc;
    char data[];
} ProcessSpawnRequest;

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#define ECAM_BASE_ADDRESS ((0x9000000000))

static PCIBusDriver bus_driver = {0};
static uint64_t pci_bus_device_id = 0;

extern uint64_t __syscall_capabilities[];

static int64_t spawn_process_via_system(const uint64_t stack_size,
                                        const uint16_t capc,
                                        const InitCapability *capabilities,
                                        const uint16_t argc,
                                        const char *argv[]) {
    const SyscallResult find_channel_result =
            anos_find_named_channel("SYSTEM::PROCESS");

    const uint64_t system_process_channel = find_channel_result.value;

    if (find_channel_result.result != SYSCALL_OK || !system_process_channel) {
        printf("ERROR: Could not find SYSTEM::PROCESS channel\n");
        return -1;
    }

    const size_t capabilities_size = capc * sizeof(InitCapability);
    size_t argv_size = 0;

    for (uint16_t i = 0; i < argc; i++) {
        if (argv[i]) {
            argv_size += strlen(argv[i]) + 1;
        }
    }

    const size_t total_size =
            sizeof(ProcessSpawnRequest) + capabilities_size + argv_size;

    static char __attribute__((aligned(4096))) ipc_buffer[4096];
    char *buffer = ipc_buffer;

    if (total_size > sizeof(ipc_buffer)) {
        printf("ERROR: Message too large (%zu > %zu)\n", total_size,
               sizeof(ipc_buffer));
        return -2;
    }

    ProcessSpawnRequest *req = (ProcessSpawnRequest *)buffer;
    req->stack_size = stack_size;
    req->argc = argc;
    req->capc = capc;

    char *data_ptr = req->data;

    if (capc > 0 && capabilities) {
        memcpy(data_ptr, capabilities, capabilities_size);
        data_ptr += capabilities_size;
    }

    if (argc > 0 && argv) {
        for (uint16_t i = 0; i < argc; i++) {
            if (argv[i]) {
                const size_t len = strlen(argv[i]);
                memcpy(data_ptr, argv[i], len);
                data_ptr[len] = '\0';
                data_ptr += len + 1;
            }
        }
    }

#ifdef DEBUG_AHCI_SPAWN
    printf("Sending process spawn request (total_size=%ld)\n", total_size);
#endif

    const SyscallResult spawn_result = anos_send_message(
            system_process_channel, PROCESS_SPAWN, total_size, buffer);

    if (spawn_result.result != SYSCALL_OK) {
        return spawn_result.result;
    }

    return (int64_t)spawn_result.value;
}

void spawn_xhci_driver(const uint64_t xhci_base, const uint64_t pci_config_base,
                       const uint64_t pci_device_id) {
#ifdef DEBUG_BUS_DRIVER_INIT
    printf("\nSpawning xHCI driver for controller at 0x%016lx (PCI config at "
           "0x%016lx)...\n",
           xhci_base, pci_config_base);
#endif

    char xhci_base_str[32];
    char pci_config_str[32];
    snprintf(xhci_base_str, sizeof(xhci_base_str), "%lx", xhci_base);
    snprintf(pci_config_str, sizeof(pci_config_str), "%lx", pci_config_base);

    char pci_device_id_str[32];
    snprintf(pci_device_id_str, sizeof(pci_device_id_str), "%lu",
             pci_device_id);

    const char *argv[] = {"boot:/xhcidrv.elf", xhci_base_str, pci_config_str,
                          pci_device_id_str};

    const InitCapability xhci_caps[] = {
            {.capability_id = SYSCALL_ID_DEBUG_PRINT,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
            {.capability_id = SYSCALL_ID_DEBUG_CHAR,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR]},
            {.capability_id = SYSCALL_ID_SLEEP,
             .capability_cookie = __syscall_capabilities[SYSCALL_ID_SLEEP]},
            {.capability_id = SYSCALL_ID_MAP_PHYSICAL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
            {.capability_id = SYSCALL_ID_MAP_VIRTUAL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
            {.capability_id = SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_ALLOC_PHYSICAL_PAGES]},
            {.capability_id = SYSCALL_ID_KILL_CURRENT_TASK,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]},
            {.capability_id = SYSCALL_ID_ALLOC_INTERRUPT_VECTOR,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_ALLOC_INTERRUPT_VECTOR]},
            {.capability_id = SYSCALL_ID_WAIT_INTERRUPT,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_WAIT_INTERRUPT]},
            {.capability_id = SYSCALL_ID_FIND_NAMED_CHANNEL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL]},
            {.capability_id = SYSCALL_ID_SEND_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]},
            {.capability_id = SYSCALL_ID_RECV_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE]},
            {.capability_id = SYSCALL_ID_REPLY_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE]},
            {.capability_id = SYSCALL_ID_CREATE_CHANNEL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL]},
            {.capability_id = SYSCALL_ID_CREATE_REGION,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_CREATE_REGION]},
    };

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("  --> spawn: %s %s\n", argv[0], argv[1]);
#endif

    const int64_t pid =
            spawn_process_via_system(0x100000, 15, xhci_caps, 4, argv);
    if (pid > 0) {
#ifdef DEBUG_BUS_DRIVER_INIT
        printf("  --> xHCI driver spawned with PID %ld\n", pid);
#endif
    } else {
        printf("ERROR: Failed to spawn xHCI driver (error code: %ld)\n", pid);
    }
}

void spawn_ahci_driver(const uint64_t ahci_base, const uint64_t pci_config_base,
                       const uint64_t pci_device_id) {
#ifdef DEBUG_BUS_DRIVER_INIT
    printf("\nSpawning AHCI driver for controller at 0x%016lx (PCI config at "
           "0x%016lx)...\n",
           ahci_base, pci_config_base);
#endif

    char ahci_base_str[32];
    char pci_config_str[32];
    snprintf(ahci_base_str, sizeof(ahci_base_str), "%lx", ahci_base);
    snprintf(pci_config_str, sizeof(pci_config_str), "%lx", pci_config_base);

    char pci_device_id_str[32];
    snprintf(pci_device_id_str, sizeof(pci_device_id_str), "%lu",
             pci_device_id);

    const char *argv[] = {"boot:/ahcidrv.elf", ahci_base_str, pci_config_str,
                          pci_device_id_str};

    const InitCapability ahci_caps[] = {
            {.capability_id = SYSCALL_ID_DEBUG_PRINT,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
            {.capability_id = SYSCALL_ID_DEBUG_CHAR,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR]},
            {.capability_id = SYSCALL_ID_SLEEP,
             .capability_cookie = __syscall_capabilities[SYSCALL_ID_SLEEP]},
            {.capability_id = SYSCALL_ID_MAP_PHYSICAL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
            {.capability_id = SYSCALL_ID_MAP_VIRTUAL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
            {.capability_id = SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_ALLOC_PHYSICAL_PAGES]},
            {.capability_id = SYSCALL_ID_KILL_CURRENT_TASK,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]},
            {.capability_id = SYSCALL_ID_ALLOC_INTERRUPT_VECTOR,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_ALLOC_INTERRUPT_VECTOR]},
            {.capability_id = SYSCALL_ID_WAIT_INTERRUPT,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_WAIT_INTERRUPT]},
            {.capability_id = SYSCALL_ID_FIND_NAMED_CHANNEL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL]},
            {.capability_id = SYSCALL_ID_SEND_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]},
            {.capability_id = SYSCALL_ID_RECV_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE]},
            {.capability_id = SYSCALL_ID_REPLY_MESSAGE,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE]},
            {.capability_id = SYSCALL_ID_CREATE_CHANNEL,
             .capability_cookie =
                     __syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL]},
    };

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("  --> spawn: %s %s\n", argv[0], argv[1]);
#endif

    const int64_t pid =
            spawn_process_via_system(0x100000, 14, ahci_caps, 4, argv);
    if (pid > 0) {
#ifdef DEBUG_BUS_DRIVER_INIT
        printf("  --> AHCI driver spawned with PID %ld\n", pid);
#endif
    } else {
        printf("ERROR: Failed to spawn AHCI driver (error code: %ld)\n", pid);
    }
}

static int pci_initialize_driver(const uint64_t ecam_base,
                                 const uint16_t segment,
                                 const uint8_t bus_start,
                                 const uint8_t bus_end) {
#ifdef DEBUG_BUS_DRIVER_INIT
    printf("Initializing PCI bus driver:\n");
    printf("  ECAM Base: 0x%016lx\n", ecam_base);
    printf("  Segment: %u\n", segment);
    printf("  Bus Range: %u-%u\n", bus_start, bus_end);
#endif

    bus_driver.ecam_base = ecam_base;
    bus_driver.segment = segment;
    bus_driver.bus_start = bus_start;
    bus_driver.bus_end = bus_end;

    // Calculate size needed (1MB per bus)
    const uint32_t num_buses = bus_end - bus_start + 1;
    bus_driver.mapped_size = num_buses * 1024 * 1024;

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("  Mapping %lu MB of ECAM space...\n",
           bus_driver.mapped_size / (1024 * 1024));
#endif

    // Map the ECAM space
    const SyscallResult result = anos_map_physical(
            ecam_base, (void *)ECAM_BASE_ADDRESS, bus_driver.mapped_size,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (result.result != SYSCALL_OK) {
        printf("Failed to map ECAM space! Error: %ld\n", result.result);
        return -1;
    }

    bus_driver.mapped_ecam = (volatile void *)ECAM_BASE_ADDRESS;

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("ECAM mapping successful at virtual address 0x%lx\n",
           (uintptr_t)bus_driver.mapped_ecam);
#endif

    return 0;
}

static void pci_enumerate_all_buses(uint64_t pci_bus_device_id) {
    // TODO maybe don't run through all the buses?
    //      should only have one root bus, can recursively scan from there...
    for (uint16_t bus = bus_driver.bus_start; bus <= bus_driver.bus_end;
         bus++) {
        pci_enumerate_bus(&bus_driver, bus, pci_bus_device_id);
    }
}

int main(const int argc, char **argv) {
    printf("\nPCI Bus Driver #%s [libanos #%s]\n\n", VERSION,
           libanos_version());

    if (argc < 5) {
        printf("Usage: %s <ecam_base> <segment> <bus_start> <bus_end>\n",
               argv[0]);
        printf("Arguments provided: %d\n", argc);
        for (int i = 0; i < argc; i++) {
            printf("  argv[%d]: %s\n", i, argv[i]);
        }
        return 1;
    }

    const uint64_t ecam_base = strtoull(argv[1], nullptr, 16);
    const uint16_t segment = (uint16_t)strtoul(argv[2], nullptr, 10);
    const uint8_t bus_start = (uint8_t)strtoul(argv[3], nullptr, 10);
    const uint8_t bus_end = (uint8_t)strtoul(argv[4], nullptr, 10);

    if (bus_start > bus_end) {
        printf("Invalid bus range: %u-%u\n", bus_start, bus_end);
        return 1;
    }

    if (pci_initialize_driver(ecam_base, segment, bus_start, bus_end) != 0) {
        printf("Failed to initialize PCI driver\n");
        return 1;
    }

    // Test DEVMAN connection for device registration
    const SyscallResult devman_result = anos_find_named_channel("DEVMAN");
    if (devman_result.result == SYSCALL_OK && devman_result.value) {
        // Register PCI bus itself as a device
        // TODO: This should be done per-bus, not per-driver
        pci_bus_device_id = 1; // For now, assume bus device ID 1
    } else {
        printf("WARN: DEVMAN channel not available - devices will not be "
               "registered\n");
    }

    pci_enumerate_all_buses(pci_bus_device_id);

    printf("\nPCI enumeration complete, PCI driver exiting.\n");
    return 0;
}