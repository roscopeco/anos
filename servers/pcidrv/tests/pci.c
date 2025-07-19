/*
 * Tests for low-level PCI routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "munit.h"
#include "pci.h"

static MunitResult test_PCI_REG_UU_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_UU_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_UU_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_UU_B(0x12345678);
    munit_assert_uint8(result, ==, 0x12);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_UM_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_UM_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_UM_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_UM_B(0x12345678);
    munit_assert_uint8(result, ==, 0x34);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_LM_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_LM_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_LM_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_LM_B(0x12345678);
    munit_assert_uint8(result, ==, 0x56);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_LL_B(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_REG_LL_B(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_REG_LL_B(0xffffffff);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_REG_LL_B(0x12345678);
    munit_assert_uint8(result, ==, 0x78);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/pci/PCI_REG_UU_B", test_PCI_REG_UU_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_UM_B", test_PCI_REG_UM_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LM_B", test_PCI_REG_LM_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LL_B", test_PCI_REG_LL_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
