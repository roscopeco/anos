/*
 * Mock implementation of the HPET kdriver for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_KERNEL_DRIVERS_H
#define __ANOS_TESTS_TEST_KERNEL_DRIVERS_H

#include <stdint.h>

#include "acpitables.h"

void mock_kernel_drivers_reset(void);

ACPI_RSDT *mock_kernel_drivers_get_last_hpet_init_rsdt(void);
uint8_t mock_kernel_drivers_get_hpet_init_call_count(void);

#endif //__ANOS_TESTS_TEST_KERNEL_DRIVERS_H