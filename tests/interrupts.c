/*
 * Tests for interrupt / IDT support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "interrupts.h"
#include "munit.h"

static void test_isr() {}

static MunitResult test_idt_attr(const MunitParameter params[], void *param) {
    uint8_t attr;

    // Not present, DPL0, type 0
    attr = idt_attr(0, 0, 0);
    munit_assert_int(attr, ==, 0x00);

    // Present, DPL0, type 0
    attr = idt_attr(1, 0, 0);
    munit_assert_int(attr, ==, 0x80);

    // Present, DPL1, type 0
    attr = idt_attr(1, 1, 0);
    munit_assert_int(attr, ==, 0xA0);

    // Present, DPL2, type 0
    attr = idt_attr(1, 2, 0);
    munit_assert_int(attr, ==, 0xC0);

    // Present, DPL3, type 0
    attr = idt_attr(1, 3, 0);
    munit_assert_int(attr, ==, 0xE0);

    // type > 1 rolls back to 1
    attr = idt_attr(2, 1, 0);
    munit_assert_int(attr, ==, 0xA0);

    // DPL 4 rolls back to zero
    attr = idt_attr(1, 4, 0);
    munit_assert_int(attr, ==, 0x80);

    // Type 1 works (even if not valid 😅)
    attr = idt_attr(1, 4, 0x1);
    munit_assert_int(attr, ==, 0x81);

    // Type E works (it is valid 🎉)
    attr = idt_attr(1, 4, 0xE);
    munit_assert_int(attr, ==, 0x8E);

    // Type F works too (it is also valid 🥳)
    attr = idt_attr(1, 4, 0xF);
    munit_assert_int(attr, ==, 0x8F);

    // Higher bits in type are ignored
    attr = idt_attr(1, 4, 0x3F);
    munit_assert_int(attr, ==, 0x8F);

    return MUNIT_OK;
}

static MunitResult test_idt_entry_addr(const MunitParameter params[],
                                       void *param) {
    IdtEntry test_entry;
    idt_entry(&test_entry, (void *)0xA0A0A0A05555AAAA, 0x1, 0x2, 0x3);

    munit_assert_short(test_entry.isr_low, ==, 0xAAAA);
    munit_assert_short(test_entry.isr_mid, ==, 0x5555);
    munit_assert_int(test_entry.isr_high, ==, 0xA0A0A0A0);
    munit_assert_int(test_entry.segment, ==, 0x1);
    munit_assert_int(test_entry.ist_entry, ==, 0x2);
    munit_assert_int(test_entry.attr, ==, 0x3);

    return MUNIT_OK;
}

static MunitResult test_idt_entry_func(const MunitParameter params[],
                                       void *param) {
    uint64_t addr = (uint64_t)test_isr;
    uint16_t addr_low = addr & 0xFFFF;
    uint16_t addr_mid = (addr & 0xFFFF0000) >> 16;
    uint32_t addr_high = (addr & 0xFFFFFFFF00000000) >> 32;

    IdtEntry test_entry;
    idt_entry(&test_entry, test_isr, 0x1, 0x2, 0x3);

    munit_assert_short(test_entry.isr_low, ==, addr_low);
    munit_assert_short(test_entry.isr_mid, ==, addr_mid);
    munit_assert_int(test_entry.isr_high, ==, addr_high);
    munit_assert_int(test_entry.segment, ==, 0x1);
    munit_assert_int(test_entry.ist_entry, ==, 0x2);
    munit_assert_int(test_entry.attr, ==, 0x3);

    return MUNIT_OK;
}

static MunitResult test_idt_r(const MunitParameter params[], void *param) {
    Idtr test_r;
    idt_r(&test_r, 0x12345678, 0xA0A0);

    munit_assert_short(test_r.limit, ==, 0xA0A0);
    munit_assert_int(test_r.base, ==, 0x12345678);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/interrupts/test_idt_attr", test_idt_attr, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/interrupts/test_idt_entry_addr", test_idt_entry_addr, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/interrupts/test_idt_entry_func", test_idt_entry_func, NULL,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/interrupts/test_idt_r", test_idt_r, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}