/*
 * stage3 - Basic driver support
 * anos - An Operating System
 * 
 * Copyright (c) 2023 Ross Bamford
 * 
 * Kernel drivers are only for very basic system hardware
 * that really *has* to be in the kernel - APICs, for example.
 * 
 * Kernel drivers get mapped into the virtual space immediately
 * after the ACPI tables, 0xFFFFFFFF81008000.
 */

#ifndef __ANOS_KERNEL_DRIVERS_H
#define __ANOS_KERNEL_DRIVERS_H

#include <stdint.h>

#define KERNEL_HARDWARE_VADDR_BASE       0xFFFF800000000000
#define KERNEL_DRIVER_VADDR_BASE         0xFFFFFFFF81008000

typedef uint64_t (*KDriverEntrypoint)(void *arg);

typedef struct _KernelDriver {
    struct _KernelDriver*   first_child;
    struct _KernelDriver*   next_sibling;
    const char              ident[30];
    const char              manufacturer[20];
    KDriverEntrypoint       entrypoint;
} KernelDriver;

void init_kernel_drivers(BIOS_SDTHeader *rsdp);

#endif//__ANOS_KERNEL_DRIVERS_H
