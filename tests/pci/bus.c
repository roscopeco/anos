/*
 * Tests for low-level PCI routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "munit.h"
#include "pci/bus.h"
#include "test_machine.h"

static MunitResult test_PCI_ADDR_ENABLE(const MunitParameter params[],
                                        void *param) {
    uint32_t result;

    result = PCI_ADDR_ENABLE(0x80000000);
    munit_assert_uint8(result, ==, 1);

    result = PCI_ADDR_ENABLE(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_ENABLE(0xffffffff);
    munit_assert_uint8(result, ==, 1);

    result = PCI_ADDR_ENABLE(0x8fffffff);
    munit_assert_uint8(result, ==, 1);

    result = PCI_ADDR_ENABLE(0x7fffffff);
    munit_assert_uint8(result, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_PCI_ADDR_BUS(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_ADDR_BUS(0x00ff0000);
    munit_assert_uint8(result, ==, 0xff);

    result = PCI_ADDR_BUS(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_BUS(0x12345678);
    munit_assert_uint8(result, ==, 0x34);

    return MUNIT_OK;
}

static MunitResult test_PCI_ADDR_DEVICE(const MunitParameter params[],
                                        void *param) {
    uint32_t result;

    result = PCI_ADDR_DEVICE(0xffffffff);
    munit_assert_uint8(result, ==, 0x1f);

    result = PCI_ADDR_DEVICE(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_DEVICE(0x12345678);
    munit_assert_uint8(result, ==, 0x0a); /* 0b01010 = 0x0a */

    result = PCI_ADDR_DEVICE(0x12345878);
    munit_assert_uint8(result, ==, 0x0b); /* 0b01011 = 0x0b */

    return MUNIT_OK;
}

static MunitResult test_PCI_ADDR_FUNC(const MunitParameter params[],
                                      void *param) {
    uint32_t result;

    result = PCI_ADDR_FUNC(0x00000700);
    munit_assert_uint8(result, ==, 0x07);

    result = PCI_ADDR_FUNC(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_FUNC(0x00000800);
    munit_assert_uint8(result, ==, 0x00);

    result = PCI_ADDR_FUNC(0x00000f00);
    munit_assert_uint8(result, ==, 0x07);

    result = PCI_ADDR_FUNC(0x00000100);
    munit_assert_uint8(result, ==, 0x01);

    result = PCI_ADDR_FUNC(0x00000200);
    munit_assert_uint8(result, ==, 0x02);

    return MUNIT_OK;
}

static MunitResult test_PCI_ADDR_REG(const MunitParameter params[],
                                     void *param) {
    uint32_t result;

    result = PCI_ADDR_REG(0x00000000);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_REG(0x00000001);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_REG(0x00000002);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_REG(0x00000003);
    munit_assert_uint8(result, ==, 0);

    result = PCI_ADDR_REG(0x00000004);
    munit_assert_uint8(result, ==, 1);

    result = PCI_ADDR_REG(0x00000007);
    munit_assert_uint8(result, ==, 1);

    result = PCI_ADDR_REG(0x00000008);
    munit_assert_uint8(result, ==, 2);

    result = PCI_ADDR_REG(0x000000fc);
    munit_assert_uint8(result, ==, 0x3f);

    result = PCI_ADDR_REG(0x000000fd);
    munit_assert_uint8(result, ==, 0x3f);

    result = PCI_ADDR_REG(0x000000fe);
    munit_assert_uint8(result, ==, 0x3f);

    result = PCI_ADDR_REG(0x000000ff);
    munit_assert_uint8(result, ==, 0x3f);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_HIGH_W(const MunitParameter params[],
                                       void *param) {
    uint32_t result;

    result = PCI_REG_HIGH_W(0x00000000);
    munit_assert_uint16(result, ==, 0);

    result = PCI_REG_HIGH_W(0xffffffff);
    munit_assert_uint16(result, ==, 0xffff);

    result = PCI_REG_HIGH_W(0x12345678);
    munit_assert_uint16(result, ==, 0x1234);

    return MUNIT_OK;
}

static MunitResult test_PCI_REG_LOW_W(const MunitParameter params[],
                                      void *param) {
    uint32_t result;

    result = PCI_REG_LOW_W(0x00000000);
    munit_assert_uint16(result, ==, 0);

    result = PCI_REG_LOW_W(0xffffffff);
    munit_assert_uint16(result, ==, 0xffff);

    result = PCI_REG_LOW_W(0x12345678);
    munit_assert_uint16(result, ==, 0x5678);

    return MUNIT_OK;
}

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

static MunitResult test_pci_address_reg_all_0(const MunitParameter params[],
                                              void *param) {

    uint32_t result = pci_address_reg(0, 0, 0, 0);

    munit_assert_uint32(result, ==, 0x80000000);

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_all_valid_buses(const MunitParameter params[],
                                     void *param) {

    for (int i = 0; i < 256; i++) {
        uint8_t bus = i;

        uint32_t result = pci_address_reg(bus, 0, 0, 0);

        munit_assert_uint32(PCI_ADDR_BUS(result), ==, i);
    }

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_out_of_bounds_buses(const MunitParameter params[],
                                         void *param) {

    uint32_t result;

    result = pci_address_reg(-1, 0, 0, 0);
    munit_assert_uint32(PCI_ADDR_BUS(result), !=, -1);

    result = pci_address_reg((uint8_t)256, 0, 0, 0);
    munit_assert_uint32(PCI_ADDR_BUS(result), !=, 256);

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_all_valid_devices(const MunitParameter params[],
                                       void *param) {

    for (int i = 0; i < 32; i++) {
        uint8_t device = i;

        uint32_t result = pci_address_reg(0, device, 0, 0);

        munit_assert_uint32(PCI_ADDR_DEVICE(result), ==, i);
    }

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_out_of_bounds_devices(const MunitParameter params[],
                                           void *param) {

    uint32_t result;

    result = pci_address_reg(0, -1, 0, 0);
    munit_assert_uint32(PCI_ADDR_DEVICE(result), !=, -1);

    result = pci_address_reg(0, 32, 0, 0);
    munit_assert_uint32(PCI_ADDR_DEVICE(result), !=, 32);

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_all_valid_funcs(const MunitParameter params[],
                                     void *param) {

    for (int i = 0; i < 8; i++) {
        uint8_t func = i;

        uint32_t result = pci_address_reg(0, 0, func, 0);

        munit_assert_uint32(PCI_ADDR_FUNC(result), ==, i);
    }

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_out_of_bounds_funcs(const MunitParameter params[],
                                         void *param) {

    uint32_t result;

    result = pci_address_reg(0, 0, -1, 0);
    munit_assert_uint32(PCI_ADDR_FUNC(result), !=, -1);

    result = pci_address_reg(0, 0, 8, 0);
    munit_assert_uint32(PCI_ADDR_FUNC(result), !=, 8);

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_all_valid_registers(const MunitParameter params[],
                                         void *param) {

    for (int i = 0; i < 64; i++) {
        uint8_t reg = i;

        uint32_t result = pci_address_reg(0, 0, 0, i);

        munit_assert_uint32((result & 0xff), ==, (i << 2));
        munit_assert_uint32(PCI_ADDR_REG(result), ==, i);
    }

    return MUNIT_OK;
}

static MunitResult
test_pci_address_reg_out_of_bounds_registers(const MunitParameter params[],
                                             void *param) {

    uint32_t result;

    result = pci_address_reg(0, 0, 0, -1);
    munit_assert_uint32(PCI_ADDR_REG(result), !=, -1);

    result = pci_address_reg(0, 0, 0, 64);
    munit_assert_uint32(PCI_ADDR_REG(result), !=, 64);

    return MUNIT_OK;
}

static MunitResult
test_pci_config_read_dword_all_0(const MunitParameter params[], void *param) {
    // Given...
    test_machine_write_inl_buffer(0xcfc, 0x12345678);

    // When...
    uint32_t result = pci_config_read_dword(0, 0, 0, 0);

    // Then...
    //      Address correctly written to address output port
    uint32_t address_written = test_machine_read_outl_buffer(0xcf8);
    munit_assert_uint32(address_written, ==, pci_address_reg(0, 0, 0, 0));

    //      And result correctly read from data input port
    munit_assert_uint32(result, ==, 0x12345678);

    return MUNIT_OK;
}

static MunitResult
test_pci_config_read_dword_values(const MunitParameter params[], void *param) {
    // Given...
    test_machine_write_inl_buffer(0xcfc, 0x12345678);

    // When...
    uint32_t result = pci_config_read_dword(0x12, 0x34, 0x56, 0x78);

    // Then...
    //      Address correctly written to address output port
    uint32_t address_written = test_machine_read_outl_buffer(0xcf8);
    munit_assert_uint32(address_written, ==,
                        pci_address_reg(0x12, 0x34, 0x56, 0x78));

    //      And result correctly read from data input port
    munit_assert_uint32(result, ==, 0x12345678);

    return MUNIT_OK;
}

static void *test_machine_setup(const MunitParameter params[],
                                void *user_data) {
    test_machine_reset();
    return NULL;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/pci/PCI_ADDR_ENABLE", test_PCI_ADDR_ENABLE, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_ADDR_BUS", test_PCI_ADDR_BUS, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_ADDR_DEVICE", test_PCI_ADDR_DEVICE, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_ADDR_FUNC", test_PCI_ADDR_FUNC, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_ADDR_REG", test_PCI_ADDR_REG, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/pci/PCI_REG_HIGH_W", test_PCI_REG_HIGH_W, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LOW_W", test_PCI_REG_LOW_W, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_UU_B", test_PCI_REG_UU_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_UM_B", test_PCI_REG_UM_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LM_B", test_PCI_REG_LM_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/PCI_REG_LL_B", test_PCI_REG_LL_B, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/pci/address_reg_all_0", test_pci_address_reg_all_0, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_all_valid_buses",
         test_pci_address_reg_all_valid_buses, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_out_of_bounds_buses",
         test_pci_address_reg_out_of_bounds_buses, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_all_valid_devs",
         test_pci_address_reg_all_valid_devices, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_out_of_bounds_devs",
         test_pci_address_reg_out_of_bounds_devices, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_all_valid_funcs",
         test_pci_address_reg_all_valid_funcs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_out_of_bounds_funcs",
         test_pci_address_reg_out_of_bounds_funcs, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_all_valid_regs",
         test_pci_address_reg_all_valid_registers, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/address_reg_out_of_bounds_regs",
         test_pci_address_reg_out_of_bounds_registers, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {(char *)"/pci/config_read_dword_all_0",
         test_pci_config_read_dword_all_0, test_machine_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/pci/config_read_dword_values",
         test_pci_config_read_dword_values, test_machine_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},

        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}
