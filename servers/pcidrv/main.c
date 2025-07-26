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

static PCIBusDriver bus_driver = {0};

extern uint64_t __syscall_capabilities[];

static int64_t spawn_process_via_system(const uint64_t stack_size,
                                        const uint16_t capc,
                                        const InitCapability *capabilities,
                                        const uint16_t argc,
                                        const char *argv[]) {
    uint64_t system_process_channel =
            anos_find_named_channel("SYSTEM::PROCESS");
    if (!system_process_channel) {
        printf("ERROR: Could not find SYSTEM::PROCESS channel\n");
        return -1;
    }

    size_t capabilities_size = capc * sizeof(InitCapability);
    size_t argv_size = 0;

    for (uint16_t i = 0; i < argc; i++) {
        if (argv[i]) {
            argv_size += strlen(argv[i]) + 1;
        }
    }

    size_t total_size =
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
                size_t len = strlen(argv[i]);
                memcpy(data_ptr, argv[i], len);
                data_ptr[len] = '\0';
                data_ptr += len + 1;
            }
        }
    }

#ifdef DEBUG_AHCI_SPAWN
    printf("Sending process spawn request (total_size=%ld)\n", total_size);
#endif

    const uint64_t response = anos_send_message(
            system_process_channel, PROCESS_SPAWN, total_size, buffer);

    return (int64_t)response;
}

void spawn_ahci_driver(const uint64_t ahci_base) {
#ifdef DEBUG_BUS_DRIVER_INIT
    printf("\nSpawning AHCI driver for controller at 0x%016lx...\n", ahci_base);
#endif

    char ahci_base_str[32];
    snprintf(ahci_base_str, sizeof(ahci_base_str), "%lx", ahci_base);

    const char *argv[] = {"boot:/ahcidrv.elf", ahci_base_str};

    InitCapability ahci_caps[] = {
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
    };

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("  --> spawn: %s %s\n", argv[0], argv[1]);
#endif

    int64_t pid = spawn_process_via_system(0x100000, 7, ahci_caps, 2, argv);
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
            ecam_base, (void *)0x9000000000, bus_driver.mapped_size,
            ANOS_MAP_VIRTUAL_FLAG_READ);
    if (result != SYSCALL_OK) {
        printf("Failed to map ECAM space! Error: %d\n", result);
        return -1;
    }

    bus_driver.mapped_ecam = (void *)0x9000000000;

#ifdef DEBUG_BUS_DRIVER_INIT
    printf("ECAM mapping successful at virtual address 0x%lx\n",
           (uintptr_t)bus_driver.mapped_ecam);
#endif

    return 0;
}

static void pci_enumerate_all_buses(void) {
    // TODO maybe don't run through all the buses?
    //      should only have one root bus, can recursively scan from there...
    for (uint16_t bus = bus_driver.bus_start; bus <= bus_driver.bus_end;
         bus++) {
        pci_enumerate_bus(&bus_driver, bus);
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

    pci_enumerate_all_buses();

    printf("\nPCI enumeration complete, PCI driver exiting.\n");
    return 0;
}