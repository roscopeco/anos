/*
 * Tests for ACPI table mapper / parser
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "acpitables.h"
#include "munit.h"

static BIOS_RSDP bad_checksum = {
        .signature = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '},
        .checksum = 42,
        .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
        .revision = 0,
        .rsdt_address = 9999,
};

static BIOS_RSDP good_checksum = {
        .signature = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '},
        .checksum = 59,
        .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
        .revision = 0,
        .rsdt_address = 9999,
};

static MunitResult test_map_null(const MunitParameter params[], void *param) {
    BIOS_SDTHeader *result = map_acpi_tables(NULL);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_map_bad_checksum_r0(const MunitParameter params[],
                                            void *param) {

    BIOS_SDTHeader *result = map_acpi_tables(&bad_checksum);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/acpitables/map_null", test_map_null, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/acpitables/bad_checksum_r0", test_map_bad_checksum_r0, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}

void vmm_map_page_containing(uintptr_t virt_addr, uint64_t phys_addr,
                             uint16_t flags) {}