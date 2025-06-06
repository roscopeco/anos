/*
 * stage3 - Kernel driver management
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stddef.h>
#include <stdint.h>

#include "kdrivers/drivers.h"
#include "platform/acpi/acpitables.h"
#include "vmm/vmconfig.h"
#include "x86_64/kdrivers/hpet.h"

#define KERNEL_DRIVER_VADDR_END                                                \
    ((KERNEL_DRIVER_VADDR_BASE + KERNEL_DRIVER_VADDR_SIZE))
#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct {
    uint8_t page[VM_PAGE_SIZE];
} DriverPage;

static DriverPage *next_page = (DriverPage *)KERNEL_DRIVER_VADDR_BASE;

bool kernel_drivers_init(ACPI_RSDT *rsdt) {
    if (!rsdt) {
        return false;
    }

    hpet_init(rsdt);

    return true;
}

void *kernel_drivers_alloc_pages(uint64_t count) {
    if (count == 0) {
        return NULL;
    }

    if (((uintptr_t)(next_page + count)) > KERNEL_DRIVER_VADDR_END) {
        return NULL;
    }

    void *result = next_page;
    next_page += count;
    return result;
}

#ifdef UNIT_TESTS
void kernel_drivers_alloc_pages_reset(void) {
    next_page = (DriverPage *)KERNEL_DRIVER_VADDR_BASE;
}
#endif