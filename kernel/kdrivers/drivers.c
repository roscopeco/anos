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

void init_kernel_drivers(ACPI_RSDT *rsdt) { init_hpet(rsdt); }
