/*
 * Tests for virtual memory TLB shootdown
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "munit.h"

#include "process.h"
#include "smp/ipwi.h"
#include "task.h"
#include "vmm/shootdown.h"

#include "mock_pagetables.h"

// === Mocks & flags for verification ===
static bool mock_map_called = false;
static bool mock_unmap_called = false;
static bool ipi_enqueued = false;
static uintptr_t last_virt_addr = 0;
static uintptr_t last_phys_addr = 0;
static uintptr_t last_flags = 0;
static size_t last_page_count = 0;
static uintptr_t last_target_pml4 = 0;

static uintptr_t last_ipwi_virt_addr = 0;
static size_t last_ipwi_page_count = 0;
static uint64_t last_ipwi_target_pid = 0;
static uintptr_t last_ipwi_target_pml4 = 0;

bool vmm_map_page_containing_in(uint64_t *pml4, uintptr_t v, uint64_t p, uint16_t f) {
    mock_map_called = true;
    last_virt_addr = v;
    last_phys_addr = p;
    last_flags = f;
    last_page_count = 1;
    last_target_pml4 = (uintptr_t)pml4;

    return true;
}

bool vmm_map_pages_containing_in(uint64_t *pml4, uintptr_t v, uint64_t p, uint16_t f, size_t n) {
    mock_map_called = true;
    last_virt_addr = v;
    last_phys_addr = p;
    last_flags = f;
    last_page_count = n;
    last_target_pml4 = (uintptr_t)pml4;

    return true;
}

uintptr_t vmm_unmap_page_in(uint64_t *pml4, const uintptr_t v) {
    mock_unmap_called = true;
    last_virt_addr = v;
    last_page_count = 1;
    last_target_pml4 = (uintptr_t)pml4;

    return 0xDEADBEEF;
}

uintptr_t vmm_unmap_pages_in(uint64_t *pml4, const uintptr_t v, const size_t n) {
    mock_unmap_called = true;
    last_virt_addr = v;
    last_page_count = n;
    last_target_pml4 = (uintptr_t)pml4;

    return 0xDEADBEEF + n;
}

uintptr_t vmm_virt_to_phys(const uintptr_t virt_addr) { return virt_addr ^ 0x12340000; }

void *vmm_phys_to_virt_ptr(const uintptr_t phys_addr) { return (void *)(phys_addr | 0x12340000); }

bool ipwi_enqueue_all_except_current(IpwiWorkItem *item) {
    ipi_enqueued = true;
    const IpwiPayloadTLBShootdown *payload = (IpwiPayloadTLBShootdown *)&item->payload;

    last_ipwi_page_count = payload->page_count;
    last_ipwi_virt_addr = payload->start_vaddr;
    last_ipwi_target_pid = payload->target_pid;
    last_ipwi_target_pml4 = payload->target_pml4;

    return true;
}

static Process fake_proc = {
        .pid = 42,
        .pml4 = (uintptr_t)0xCAFEB000,
};

static Task dummy_task = {
        .owner = &fake_proc,
};

Task *task_current(void) { return &dummy_task; }

uint64_t save_disable_interrupts(void) { return 0x1983; }
void restore_saved_interrupts(const uint64_t flags) { (void)flags; }

// === TESTS ===

#define RESET_FLAGS()                                                                                                  \
    do {                                                                                                               \
        mock_map_called = false;                                                                                       \
        mock_unmap_called = false;                                                                                     \
        ipi_enqueued = false;                                                                                          \
        last_virt_addr = last_phys_addr = last_flags = 0;                                                              \
        last_page_count = last_target_pml4 = 0;                                                                        \
        last_ipwi_page_count = 0;                                                                                      \
        last_ipwi_target_pid = 0;                                                                                      \
        last_ipwi_target_pml4 = 0;                                                                                     \
        last_ipwi_virt_addr = 0;                                                                                       \
    } while (0)

static MunitResult test_map_page_process(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const bool result = vmm_shootdown_map_page_containing_in_process(&fake_proc, 0x4000, 0x9000, 0x07);

    munit_assert_true(result);
    munit_assert_true(mock_map_called && ipi_enqueued);
    munit_assert_uint64(last_virt_addr, ==, 0x4000);

    return MUNIT_OK;
}

static MunitResult test_unmap_page_process(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const uintptr_t r = vmm_shootdown_unmap_page_in_process(&fake_proc, 0xC000);

    munit_assert_true(mock_unmap_called && ipi_enqueued);
    munit_assert_uint64(last_virt_addr, ==, 0xC000);
    munit_assert_uint64(r, ==, 0xDEADBEEF);

    return MUNIT_OK;
}

static MunitResult test_map_pages_pml4(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    uint64_t *pml4 = (uint64_t *)0x88888000;
    const bool result = vmm_shootdown_map_pages_containing_in_pml4(pml4, 0x7000, 0x1234, 0x05, 3);

    munit_assert_true(result);
    munit_assert_true(mock_map_called && ipi_enqueued);
    munit_assert_uint64(last_page_count, ==, 3);
    munit_assert_uint64(last_target_pml4, ==, (uintptr_t)pml4);

    return MUNIT_OK;
}

static MunitResult test_unmap_pages_current(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const uintptr_t r = vmm_shootdown_unmap_pages(0x9000, 2);

    munit_assert_true(mock_unmap_called && ipi_enqueued);
    munit_assert_uint64(last_page_count, ==, 2);
    munit_assert_uint64(r, ==, 0xDEADBEEF + 2);

    return MUNIT_OK;
}

static MunitResult test_alias_map_page(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const bool result = vmm_shootdown_map_page(0x5000, 0x6000, 0x01);

    munit_assert_true(result);
    munit_assert_true(mock_map_called && ipi_enqueued);
    munit_assert_uint64(last_virt_addr, ==, 0x5000);

    return MUNIT_OK;
}

static MunitResult test_alias_map_pages(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const bool result = vmm_shootdown_map_pages(0xD000, 0xE000, 0x03, 4);

    munit_assert_true(result);
    munit_assert_true(mock_map_called && ipi_enqueued);
    munit_assert_uint64(last_virt_addr, ==, 0xD000);
    munit_assert_uint64(last_page_count, ==, 4);

    return MUNIT_OK;
}

static MunitResult test_alias_unmap_page(const MunitParameter params[], void *data) {
    RESET_FLAGS();
    const uintptr_t r = vmm_shootdown_unmap_page(0xA000);

    munit_assert_true(mock_unmap_called && ipi_enqueued);
    munit_assert_uint64(last_virt_addr, ==, 0xA000);
    munit_assert_uint64(r, ==, 0xDEADBEEF);

    return MUNIT_OK;
}

static MunitTest shootdown_tests[] = {
        {"/map_page_process", test_map_page_process, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/unmap_page_process", test_unmap_page_process, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/map_pages_pml4", test_map_pages_pml4, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/unmap_pages_current", test_unmap_pages_current, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alias_map_page", test_alias_map_page, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alias_map_pages", test_alias_map_pages, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/alias_unmap_page", test_alias_unmap_page, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite shootdown_suite = {"/vmm/shootdown", shootdown_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(const int argc, char *argv[]) { return munit_suite_main(&shootdown_suite, NULL, argc, argv); }
