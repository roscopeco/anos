/*
 * stage3 - Mock kernel CPU driver for tests
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#include <stdbool.h>
#include <stdint.h>

#include "mock_cpu.h"

char *brand = "Mock Test CPU @ 0 GHz";

uint64_t mock_cpu_msr_value;
uint64_t mock_invlpg_count;

void mock_cpu_reset() {
    mock_cpu_msr_value = 0;
    mock_invlpg_count = 0;
}

bool cpu_init_this(void) { return true; }

uint64_t cpu_read_local_apic_id(void) { return 1; }

void cpu_tsc_delay(uint64_t cycles) {
    // noop
}

void cpu_tsc_mdelay(int n) {
    // noop
}
void cpu_tsc_udelay(int n) {
    // noop
}

// Buffer must have space for 49 characters!
void cpu_get_brand_str(char *buffer) { strncpy(buffer, brand, 49); }

void cpu_debug_info(uint8_t cpu_num) {
    // noop
}

inline uint64_t cpu_read_msr(uint32_t msr) { return mock_cpu_msr_value; }

inline void cpu_write_msr(uint64_t msr, uint64_t value) {
    // noop
}

inline uint64_t cpu_read_tsc(void) { return 1000; }

// Execute `lgdt` to load a variable with the GDTR
inline void cpu_load_gdtr(GDTR *gdtr) {
    // noop
}

// Execute `sgdt` to load GDTR from a variable
inline void cpu_store_gdtr(GDTR *gdtr) {
    // noop
}

// Execute `lidt` to load a variable with the IDTR
inline void cpu_load_idtr(IDTR *idtr) {
    // noop
}

// Execute `sidt` to load IDTR from a variable
inline void cpu_store_idtr(IDTR *idtr) {
    // noop
}

inline void cpu_invalidate_tlb_addr(uintptr_t virt_addr) {
    mock_invlpg_count++;
}

inline void cpu_swapgs(void) {
    // noop
}

uintptr_t cpu_read_cr3(void) { return 0x1234; }
