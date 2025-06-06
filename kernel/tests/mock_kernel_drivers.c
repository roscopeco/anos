/*
 * Mock implementation of the HPET kdriver for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>

#include "platform/acpi/acpitables.h"
#include "x86_64/kdrivers/hpet.h"

static ACPI_RSDT *hpet_init_last_rsdt;
static uint32_t hpet_init_call_count;

/* Mock interface */

void mock_kernel_drivers_reset(void) {
    hpet_init_call_count = 0;
    hpet_init_last_rsdt = ((void *)0);
}

ACPI_RSDT *mock_kernel_drivers_get_last_hpet_init_rsdt(void) {
    return hpet_init_last_rsdt;
}
uint32_t mock_kernel_drivers_get_hpet_init_call_count(void) {
    return hpet_init_call_count;
}

/* Driver interfaces */

bool hpet_init(ACPI_RSDT *rsdt) {
    hpet_init_last_rsdt = rsdt;
    hpet_init_call_count++;
    return true;
}
