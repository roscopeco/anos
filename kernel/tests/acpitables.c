/*
 * Tests for ACPI table initper / parser
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "acpitables.h"
#include "mock_vmm.h"
#include "munit.h"

static ACPI_RSDP rsdp_bad_checksum = {
        .signature = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '},
        .checksum = 42,
        .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
        .revision = 0,
        .rsdt_address = 0x9999,
};

static ACPI_RSDP rsdp_good_checksum = {
        .signature = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '},
        .checksum = 59,
        .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
        .revision = 0,
        .rsdt_address = 0x9999,
};

static ACPI_RSDT rsdt_good_empty = {
        .header = {.signature = {'R', 'S', 'D', 'T'},
                   .checksum = 123,
                   .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
                   .revision = 0,
                   .length = sizeof(ACPI_SDTHeader)}};

static ACPI_MADT madt_good = {
        .header = {.signature = {'M', 'A', 'D', 'T'},
                   .checksum = 123,
                   .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
                   .revision = 0,
                   .length = sizeof(ACPI_MADT)},
        .lapic_address = 0x12341234,
        .lapic_flags = 0x56789abc,
};

static ACPI_RSDT rsdt_good_madt = {
        .header =
                {
                        .signature = {'R', 'S', 'D', 'T'},
                        .checksum = 123,
                        .oem_id = {'A', 'N', 'O', 'E', 'M', 0},
                        .revision = 0,
                        .length = sizeof(ACPI_SDTHeader) + 4,
                },
        // TODO MADT not actually linked...
};

static void *test_setup(const MunitParameter params[], void *user_data) {
    mock_vmm_reset();

    return NULL;
}

static MunitResult test_init_null(const MunitParameter params[], void *param) {
    ACPI_RSDT *result = acpi_tables_init(NULL);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_init_bad_checksum_r0(const MunitParameter params[],
                                             void *param) {

    ACPI_RSDT *result = acpi_tables_init(&rsdp_bad_checksum);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_init_good_checksum_r0(const MunitParameter params[],
                                              void *param) {

    ACPI_RSDT *result = acpi_tables_init(&rsdp_good_checksum);

    munit_assert_not_null(result);

    munit_assert_uint32(mock_vmm_get_total_page_maps(), ==, 1);
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==, 0x9000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==,
                        ACPI_TABLES_VADDR_BASE);

    return MUNIT_OK;
}

static MunitResult test_find_null_rsdt(const MunitParameter params[],
                                       void *param) {

    ACPI_SDTHeader *result = acpi_tables_find(NULL, "MADT");

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_find_null_ident(const MunitParameter params[],
                                        void *param) {

    ACPI_SDTHeader *result = acpi_tables_find(&rsdt_good_madt, NULL);

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_find_good_empty(const MunitParameter params[],
                                        void *param) {

    ACPI_SDTHeader *result = acpi_tables_find(&rsdt_good_empty, "MADT");

    munit_assert_null(result);

    return MUNIT_OK;
}

static MunitResult test_find_good_only_madt(const MunitParameter params[],
                                            void *param) {

    // TODO Can't really do much more useful testing, because currently ACPI tables
    //      are limited to 32-bit (due to qemu support for >r0 tables not being there).
    //
    //      Needs revisiting once the kernel supports non-legacy boot and we
    //      move to UEFI...
    //

    // ACPI_SDTHeader *result = acpi_tables_find(&rsdt_good_madt, "MADT");

    // munit_assert_not_null(result);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {
                (char *)"/acpitables/init_null",
                test_init_null,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },
        {
                (char *)"/acpitables/bad_checksum_r0",
                test_init_bad_checksum_r0,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },
        {
                (char *)"/acpitables/good_checksum_r0",
                test_init_bad_checksum_r0,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },

        {
                (char *)"/acpitables/find_null_rsdt",
                test_find_null_rsdt,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },
        {
                (char *)"/acpitables/find_null_ident",
                test_find_null_ident,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },
        {
                (char *)"/acpitables/find_good_empty",
                test_find_good_empty,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },
        {
                (char *)"/acpitables/find_good_madt",
                test_find_good_only_madt,
                test_setup,
                NULL,
                MUNIT_TEST_OPTION_NONE,
                NULL,
        },

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
