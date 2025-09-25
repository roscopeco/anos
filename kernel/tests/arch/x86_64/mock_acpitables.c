/*
 * Mock implementation of the ACPI tables for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "platform/acpi/acpitables.h"

ACPI_SDTHeader *find_result;
uint32_t find_call_count;
ACPI_RSDT *last_find_rsdt;
char *last_find_ident;

/* Mocks */
void mock_acpitables_reset(void) {
    find_call_count = 0;
    last_find_rsdt = NULL;
    last_find_ident = NULL;
    find_result = NULL;
}

uint32_t mock_acpitables_get_acpi_tables_find_call_count(void) { return find_call_count; }
ACPI_RSDT *mock_acpitables_get_acpi_tables_get_last_find_rsdt(void) { return last_find_rsdt; }
char *mock_acpitables_get_acpi_tables_get_last_find_ident(void) { return last_find_ident; }

void mock_acpitables_set_find_result(ACPI_SDTHeader *result) { find_result = result; }

/* Interface */

ACPI_SDTHeader *acpi_tables_find(ACPI_RSDT *rsdt, const char *ident) {
    find_call_count++;
    last_find_rsdt = rsdt;
    last_find_ident = (char *)ident;
    return find_result;
}
