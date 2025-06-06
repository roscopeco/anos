/*
 * Mock implementation of the ACPI tables for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_ACPITABLES_H
#define __ANOS_TESTS_TEST_ACPITABLES_H

#include <stdint.h>

#include "platform/acpi/acpitables.h"

void mock_acpitables_reset(void);

uint32_t mock_acpitables_get_acpi_tables_find_call_count(void);
ACPI_RSDT *mock_acpitables_get_acpi_tables_get_last_find_rsdt(void);
char *mock_acpitables_get_acpi_tables_get_last_find_ident(void);

void mock_acpitables_set_find_result(ACPI_SDTHeader *result);

/* Pass varargs ACPI_SDTHeader pointers to build tables in buffer */
void build_acpi_table(ACPI_RSDT *buffer, ...);

#endif //__ANOS_TESTS_TEST_ACPITABLES_H