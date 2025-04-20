/*
 * Tests for GDT manipulation and setup routines
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */
#include "munit.h"

#include "x86_64/gdt.h"

static const GDTEntry empty_entry = {0};

static MunitResult test_init_gdt_entry(const MunitParameter params[],
                                       void *param) {
    GDTEntry entry;
    init_gdt_entry(&entry, 0x12345678, 0x19ABC, 0x92, 0xCF);

    munit_assert_uint16(entry.limit_low, ==, 0x9ABC);
    munit_assert_uint16(entry.base_low, ==, 0x5678);
    munit_assert_uint8(entry.base_middle, ==, 0x34);
    munit_assert_uint8(entry.access, ==, 0x92);
    munit_assert_uint8(entry.flags_limit_h, ==, 0xC1);
    munit_assert_uint8(entry.base_high, ==, 0x12);

    return MUNIT_OK;
}

static MunitResult test_get_gdt_entry(const MunitParameter params[],
                                      void *param) {
    GDTEntry gdt_entries[3];
    GDTEntry *entry;
    GDTR gdtr = {.limit = sizeof(gdt_entries) - 1,
                 .base = (uint64_t)gdt_entries};

    init_gdt_entry(&gdt_entries[0], 0x0, 0xFFFFF, 0x9A, 0xCF);
    init_gdt_entry(&gdt_entries[1], 0x0, 0xFFFFF, 0x92, 0xCF);
    init_gdt_entry(&gdt_entries[2], 0x0, 0xFFFFF, 0xFA, 0xCF);

    entry = get_gdt_entry(&gdtr, 0);
    munit_assert_ptr_equal(entry, &gdt_entries[0]);

    entry = get_gdt_entry(&gdtr, 1);
    munit_assert_ptr_equal(entry, &gdt_entries[1]);

    entry = get_gdt_entry(&gdtr, 2);
    munit_assert_ptr_equal(entry, &gdt_entries[2]);

    entry = get_gdt_entry(&gdtr, 3);
    munit_assert_null(entry);

    return MUNIT_OK;
}

static MunitResult test_access_macros(const MunitParameter params[],
                                      void *param) {
    GDTEntry entry;

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING0 |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING0 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING1 |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING1 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING2 |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING2 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING3 |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING3 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_RING0 | GDT_ENTRY_ACCESS_EXECUTABLE, 0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_RING0 | GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_NON_SYSTEM |
                           GDT_ENTRY_ACCESS_RING3 | GDT_ENTRY_ACCESS_READ_WRITE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_NON_SYSTEM |
                        GDT_ENTRY_ACCESS_RING3 | GDT_ENTRY_ACCESS_READ_WRITE));

    return MUNIT_OK;
}

static MunitResult test_access_dpl_macro(const MunitParameter params[],
                                         void *param) {
    GDTEntry entry;

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(0) |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING0 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(1) |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING1 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(2) |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING2 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    init_gdt_entry(&entry, 0x0, 0xFFFFF,
                   GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_DPL(3) |
                           GDT_ENTRY_ACCESS_EXECUTABLE,
                   0xCF);
    munit_assert_uint8(entry.access, ==,
                       (GDT_ENTRY_ACCESS_PRESENT | GDT_ENTRY_ACCESS_RING3 |
                        GDT_ENTRY_ACCESS_EXECUTABLE));

    return MUNIT_OK;
}

static MunitResult test_flags_macros(const MunitParameter params[],
                                     void *param) {
    GDTEntry entry;

    init_gdt_entry(&entry, 0x0, 0xFFFFF, 0x9A,
                   GDT_ENTRY_FLAGS_GRANULARITY | GDT_ENTRY_FLAGS_SIZE);
    munit_assert_uint8(entry.flags_limit_h, ==,
                       0x0F | GDT_ENTRY_FLAGS_GRANULARITY |
                               GDT_ENTRY_FLAGS_SIZE);

    init_gdt_entry(&entry, 0x0, 0xFFFFF, 0x9A, GDT_ENTRY_FLAGS_64BIT);
    munit_assert_uint8(entry.flags_limit_h, ==,
                       0x0F | GDT_ENTRY_FLAGS_LONG_MODE);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
        {(char *)"/gdt/init_gdt_entry", test_init_gdt_entry, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/gdt/get_gdt_entry", test_get_gdt_entry, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/gdt/access_macros", test_access_macros, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/gdt/access_dpl_macro", test_access_dpl_macro, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {(char *)"/gdt/flags_macros", test_flags_macros, NULL, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite test_suite = {(char *)"", test_suite_tests, NULL, 1,
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void *)"µnit", argc, argv);
}