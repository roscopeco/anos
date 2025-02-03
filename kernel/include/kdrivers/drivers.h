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

#include <stdbool.h>
#include <stdint.h>

#include "acpitables.h"

// See MemoryMap.md for details on the size and purpose of these...
#define KERNEL_HARDWARE_VADDR_BASE 0xffffffa000000000
#define KERNEL_DRIVER_VADDR_BASE 0xffffffff81020000
#define KERNEL_DRIVER_VADDR_SIZE 0x00000000000e0000

typedef uint64_t (*KDriverEntrypoint)(void *arg);

typedef struct _KernelDriver {
    struct _KernelDriver *first_child;
    struct _KernelDriver *next_sibling;
    const char ident[30];
    const char manufacturer[20];
    KDriverEntrypoint entrypoint;
} KernelDriver;

bool kernel_drivers_init(ACPI_RSDT *rsdp);

/*
 * Allocate page(s) in the kernel driver area for 
 * base system driver MMIO.
 *
 * This is a one-way street - there is no free. Since this
 * is only for the very basic drivers the kernel will init
 * at boot time, we will never need to unmap them... 
 * 
 * In the current design, there are 248 pages total 
 * (for 992KiB) of address space available here.
 */
void *kernel_drivers_alloc_pages(uint64_t count);

#endif //__ANOS_KERNEL_DRIVERS_H
