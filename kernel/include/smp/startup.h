/*
 * stage3 - SMP startup support
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_SMP_STARTUP_H
#define __ANOS_SMP_STARTUP_H

#include <stdint.h>

#include "platform/acpi/acpitables.h"

void smp_bsp_start_aps(ACPI_RSDT *rsdt, uint32_t volatile *lapic);

#endif //__ANOS_SMP_STARTUP_H
