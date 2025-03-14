/*
 * stage3 - Mock kernel CPU driver for tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_MOCK_CPU_H
#define __ANOS_KERNEL_MOCK_CPU_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "gdt.h"
#include "interrupts.h"

#define MSR_FSBase ((0xC0000100))
#define MSR_GSBase ((0xC0000101))
#define MSR_KernelGSBase ((0xC0000102))

#define CPU_TSS_ENTRY_SIZE_MULT ((2))

static char *brand = "Mock Test CPU @ 0 GHz";

#ifdef MUNIT_H
#define VAR_DECL extern
#else
#define VAR_DECL
#endif

VAR_DECL uint64_t mock_cpu_msr_value;
VAR_DECL uint64_t mock_invlpg_count;

static void mock_cpu_reset() {
    mock_cpu_msr_value = 0;
    mock_invlpg_count = 0;
}

static bool cpu_init_this(void) { return true; }

static uint64_t cpu_read_local_apic_id(void) { return 1; }

static void cpu_tsc_delay(uint64_t cycles) {
    // noop
}

static void cpu_tsc_mdelay(int n) {
    // noop
}
static void cpu_tsc_udelay(int n) {
    // noop
}

// Buffer must have space for 49 characters!
static void cpu_get_brand_str(char *buffer) { strncpy(buffer, brand, 49); }

static void cpu_debug_info(uint8_t cpu_num) {
    // noop
}

static inline uint64_t cpu_read_msr(uint32_t msr) { return mock_cpu_msr_value; }

static inline void cpu_write_msr(uint64_t msr, uint64_t value) {
    // noop
}

static inline uint64_t cpu_read_tsc(void) { return 1000; }

// Execute `lgdt` to load a variable with the GDTR
static inline void cpu_load_gdtr(GDTR *gdtr) {
    // noop
}

// Execute `sgdt` to load GDTR from a variable
static inline void cpu_store_gdtr(GDTR *gdtr) {
    // noop
}

// Execute `lidt` to load a variable with the IDTR
static inline void cpu_load_idtr(IDTR *idtr) {
    // noop
}

// Execute `sidt` to load IDTR from a variable
static inline void cpu_store_idtr(IDTR *idtr) {
    // noop
}

static inline void cpu_invalidate_page(uintptr_t virt_addr) {
    mock_invlpg_count++;
}

static inline void cpu_swapgs(void) {
    // noop
}

#endif //__ANOS_KERNEL_MOCK_CPU_H