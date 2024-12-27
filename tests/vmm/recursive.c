/*
 * Tests for recursive mapping accessors
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "vmm/recursive.h"
#include "munit.h"
#include <stddef.h>
#include <stdlib.h>

static MunitResult test_table_address_0(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(0, 0, 0, 0, 0);

    // This is actually an illegal (non-canonical) address...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff000000000000 == 0b1111111111111111 000000000 000000000 000000000 000000000 000000000000
    munit_assert_uint64(addr, ==, 0xffff000000000000);

    return MUNIT_OK;
}

static MunitResult test_table_address_511_0s(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(511, 0, 0, 0, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffffff8000000000 == 0b1111111111111111 111111111 000000000 000000000 000000000 000000000000
    munit_assert_uint64(addr, ==, 0xffffff8000000000);

    return MUNIT_OK;
}

static MunitResult test_table_address_256s(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(256, 256, 256, 256, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020100000 == 0b1111111111111111 100000000 100000000 100000000 100000000 000000000000

    //
    munit_assert_uint64(addr, ==, 0xffff804020100000);

    return MUNIT_OK;
}

static MunitResult test_table_address_256_0s(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(256, 0, 0, 0, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800000000000 == 0b1111111111111111 100000000 000000000 000000000 000000000 000000000000

    //
    munit_assert_uint64(addr, ==, 0xffff800000000000);

    return MUNIT_OK;
}

static MunitResult test_table_address_256_511s(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(256, 511, 511, 511, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff807ffffff000 == 0b1111111111111111 100000000 111111111 111111111 111111111 000000000000

    //
    munit_assert_uint64(addr, ==, 0xffff807ffffff000);

    return MUNIT_OK;
}

static MunitResult test_table_address_256_max(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(256, 511, 511, 511, 4095);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff807fffffffff == 0b1111111111111111 100000000 111111111 111111111 111111111 111111111111

    //
    munit_assert_uint64(addr, ==, 0xffff807fffffffff);

    return MUNIT_OK;
}

static MunitResult test_table_address_oob(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(511, 513, 514, 515, 4096);

    // values are clamped to relevant max and rolled over
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffffff8000000000 == 0b1111111111111111 111111111 000000001 000000010 000000011 000000000000
    munit_assert_uint64(addr, ==, 0xffffff8040403000);

    return MUNIT_OK;
}

static MunitResult test_table_address_pml4(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(511, 511, 511, 511, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xfffffffffffff000 == 0b1111111111111111 111111111 111111111 111111111 111111111 000000000000
    munit_assert_uint64(addr, ==, 0xfffffffffffff000);

    return MUNIT_OK;
}

static MunitResult test_table_address_pml4_max(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(511, 511, 511, 511, 4095);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffffffffffffffff == 0b1111111111111111 111111111 111111111 111111111 111111111 111111111111
    munit_assert_uint64(addr, ==, 0xfffff80000000000);

    return MUNIT_OK;
}

static MunitResult test_table_address_pdpt_4_pml4_2(const MunitParameter params[], void *fixture) {
    uintptr_t addr = vmm_recursive_table_address(511, 511, 4, 2, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffffffffc0802000 == 0b1111111111111111 111111111 111111111 000000100 000000010 000000000000
    munit_assert_uint64(addr, ==, 0xffffffffc0802000);

    return MUNIT_OK;
}

static MunitResult test_find_pml4(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pml4();

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020100000 == 0b1111111111111111 100000000 100000000 100000000 100000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804020100000);

    return MUNIT_OK;
}

static MunitResult test_find_pdpt_0(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pdpt(0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020000000 == 0b1111111111111111 100000000 100000000 100000000 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804020000000);

    return MUNIT_OK;
}

static MunitResult test_find_pdpt_1(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pdpt(1);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020001000 == 0b1111111111111111 100000000 100000000 100000000 000000001 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804020001000);

    return MUNIT_OK;
}

static MunitResult test_find_pdpt_511(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pdpt(511);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff8040201ff000 == 0b1111111111111111 100000000 100000000 100000000 111111111 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff8040201ff000);

    return MUNIT_OK;
}

static MunitResult test_find_pdpt_oob(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pdpt(512);

    // Should wraparound to zero...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020000000 == 0b1111111111111111 100000000 100000000 100000000 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804020000000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_0_0(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(0, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000000000 == 0b1111111111111111 100000000 100000000 000000000 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804000000000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_1_0(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(1, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000200000 == 0b1111111111111111 100000000 100000000 000000001 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804000200000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_1_511(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(1, 511);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff8040003ff000 == 0b1111111111111111 100000000 100000000 000000001 111111111 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff8040003ff000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_511_511(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(511, 511);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff80403ffff000 == 0b1111111111111111 100000000 100000000 111111111 111111111 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff80403ffff000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_1_oob(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(1, 512);

    // Should wraparound to zero...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000200000 == 0b1111111111111111 100000000 100000000 000000001 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804000200000);

    return MUNIT_OK;
}

static MunitResult test_find_pd_oob_oob(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pd(512, 512);

    // Should wraparound to zero...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000000000 == 0b1111111111111111 100000000 100000000 000000000 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804000000000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_0_0_0(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(0, 0, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800000000000 == 0b1111111111111111 100000000 000000000 000000000 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff800000000000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_0_1_0(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(0, 1, 0);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800000200000 == 0b1111111111111111 100000000 000000000 000000001 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff800000200000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_1_1_1(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(1, 1, 1);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000200000 == 0b1111111111111111 100000000 000000001 000000001 000000001 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff804000200000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_1_1_511(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(1, 1, 511);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff8000403ff000 == 0b1111111111111111 100000000 000000001 000000001 111111111 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff8000403ff000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_511_511_511(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(511, 511, 511);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff807ffffff000 == 0b1111111111111111 100000000 111111111 111111111 111111111 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff807ffffff000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_1_1_oob(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(1, 1, 512);

    // Should wraparound to zero...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800040200000 == 0b1111111111111111 100000000 000000001 000000001 000000000 000000000000
    munit_assert_ptr_equal(addr, (PageTable *)0xffff800040200000);

    return MUNIT_OK;
}

static MunitResult test_find_pt_oob_oob_oob(const MunitParameter params[], void *fixture) {
    PageTable *addr = vmm_recursive_find_pt(512, 512, 512);

    // Should wraparound to zero...
    //
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800000000000 == 0b1111111111111111 100000000 000000000 000000000 000000000 000000000000"
    munit_assert_ptr_equal(addr, (PageTable *)0xffff800000000000);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pte(const MunitParameter params[], void *fixture) {
    // Test address with known indices
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0x0000008080604000 :: 0b0000000000000000 000000001 000000010 000000011 000000100 000000000000
    uintptr_t test_addr = 0x0000008080604000;

    uint64_t *pte = vmm_virt_to_pte(test_addr);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800040403020 :: 0b1111111111111111 100000000 000000001 000000010 000000011 000000100000
    munit_assert_ptr_equal(pte, (uint64_t *)0xffff800040403020);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pt(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    PageTable *pt = vmm_virt_to_pt(test_addr);

    // Should mask off the offset bits from the PTE address
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff800040403000 :: 0b1111111111111111 100000000 000000001 000000010 000000011 00000000000
    munit_assert_ptr_equal(pt, (PageTable *)0xffff800040403000);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pde(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    uint64_t *pde = vmm_virt_to_pde(test_addr);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000202018 :: 0b1111111111111111 100000000 100000000 000000001 000000010 000000011000
    munit_assert_ptr_equal(pde, (uint64_t *)0xffff804000202018);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pd(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    PageTable *pd = vmm_virt_to_pd(test_addr);

    // Should mask off the offset bits from the PDE address
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804000202000 :: 0b1111111111111111 100000000 100000000 000000001 000000010 000000000000
    munit_assert_ptr_equal(pd, (PageTable *)0xffff804000202000);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pdpte(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    uint64_t *pdpte = vmm_virt_to_pdpte(test_addr);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020001010 :: 0b1111111111111111 100000000 100000000 100000000 000000001 000000010000
    munit_assert_ptr_equal(pdpte, (uint64_t *)0xffff804020001010);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pdpt(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    PageTable *pdpt = vmm_virt_to_pdpt(test_addr);

    // Should mask off the offset bits from the PDPTE address
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020001000 :: 0b1111111111111111 100000000 100000000 100000000 000000001 000000000000
    munit_assert_ptr_equal(pdpt, (PageTable *)0xffff804020001000);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pml4e(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;

    uint64_t *pml4e = vmm_virt_to_pml4e(test_addr);

    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020100008 :: 0b1111111111111111 100000000 100000000 100000000 100000000 000000001000
    munit_assert_ptr_equal(pml4e, (uint64_t *)0xffff804020100008);

    return MUNIT_OK;
}

static MunitResult test_virt_to_pml4(const MunitParameter params[], void *fixture) {
    uintptr_t test_addr = 0x0000008080604000;
    uintptr_t different_addr = 0x0000007070503000; // Different address should give same result

    PageTable *pml4_1 = vmm_virt_to_pml4(test_addr);
    PageTable *pml4_2 = vmm_virt_to_pml4(different_addr);

    // Should mask off the offset bits from the PML4E address and be the same for any input
    //                              Extend        PML4      PDPT       PD        PT        Offset
    // 0xffff804020100000 :: 0b1111111111111111 100000000 100000000 100000000 100000000 000000000000
    munit_assert_ptr_equal(pml4_1, (PageTable *)0xffff804020100000);
    munit_assert_ptr_equal(pml4_2, (PageTable *)0xffff804020100000);
    munit_assert_ptr_equal(pml4_1, pml4_2);

    // Should also be internally consistent...
    munit_assert_ptr_equal(pml4_1, vmm_recursive_find_pml4());

    return MUNIT_OK;
}

static MunitTest vmm_tests[] = {{"/table_address_0", test_table_address_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_511_0s", test_table_address_511_0s, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_256s", test_table_address_256s, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_256_0s", test_table_address_256_0s, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_256_511s", test_table_address_256_511s, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_256_max", test_table_address_256_max, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_oob", test_table_address_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_pml4", test_table_address_pml4, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/table_address_pdpt_4_pml4_2", test_table_address_pdpt_4_pml4_2, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {"/find_pml4", test_find_pml4, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {"/find_pdpt_0", test_find_pdpt_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pdpt_1", test_find_pdpt_1, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pdpt_511", test_find_pdpt_511, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pdpt_oob", test_find_pdpt_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {"/find_pd_0_0", test_find_pd_0_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pd_1_0", test_find_pd_1_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pd_1_511", test_find_pd_1_511, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pd_511_511", test_find_pd_511_511, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pd_1_oob", test_find_pd_1_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pd_oob_oob", test_find_pd_oob_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {"/find_pt_0_0_0", test_find_pt_0_0_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pt_0_1_0", test_find_pt_0_1_0, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pt_1_1_511", test_find_pt_1_1_511, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pt_511_511_511", test_find_pt_511_511_511, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pt_1_1_oob", test_find_pt_1_1_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/find_pt_oob_oob_oob", test_find_pt_oob_oob_oob, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {"/virt_to_pte", test_virt_to_pte, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pt", test_virt_to_pt, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pde", test_virt_to_pde, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pd", test_virt_to_pd, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pdpte", test_virt_to_pdpte, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pdpt", test_virt_to_pdpt, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pml4e", test_virt_to_pml4e, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
                                {"/virt_to_pml4", test_virt_to_pml4, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},

                                {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/vmm/∇", vmm_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) { return munit_suite_main(&test_suite, NULL, argc, argv); }