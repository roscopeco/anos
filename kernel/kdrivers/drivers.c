/*
 * stage3 - Kernel driver management
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stddef.h>

#include "acpitables.h"
#include "kdrivers/drivers.h"
#include "kdrivers/hpet.h"

bool kernel_drivers_init(ACPI_RSDT *rsdt) {
    if (!rsdt) {
        return false;
    }

    hpet_init(rsdt);

    return true;
}
