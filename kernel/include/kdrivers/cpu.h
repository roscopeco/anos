/*
 * stage3 - Kernel CPU driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_KERNEL_DRIVERS_CPU_H
#define __ANOS_KERNEL_DRIVERS_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "gdt.h"
#include "interrupts.h"

#define MSR_FSBase ((0xC0000100))
#define MSR_GSBase ((0xC0000101))
#define MSR_KernelGSBase ((0xC0000102))

#define MAX_CPU_COUNT ((16))

#define CPU_TSS_ENTRY_SIZE_MULT ((2))

bool cpu_init_this(void);

uint64_t cpu_read_local_apic_id(void);

void cpu_tsc_delay(uint64_t cycles);
void cpu_tsc_mdelay(int n);
void cpu_tsc_udelay(int n);

// Buffer must have space for 49 characters!
void cpu_get_brand_str(char *buffer);

void cpu_debug_info(uint8_t cpu_num);

static inline uint64_t cpu_read_msr(uint32_t msr) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr) : "memory");
    return ((uint64_t)edx << 32) | eax;
}

static inline void cpu_write_msr(uint64_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t cpu_read_tsc(void) {
    uint32_t edx, eax;
    __asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx) : : "memory");
    return ((uint64_t)edx << 32) | eax;
}

// Execute `lgdt` to load a variable with the GDTR
static inline void cpu_load_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("lgdt (%0)" : : "r"(gdtr));
}

// Execute `sgdt` to load GDTR from a variable
static inline void cpu_store_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("sgdt (%0)" : : "r"(gdtr) : "memory");
}

// Execute `lidt` to load a variable with the IDTR
static inline void cpu_load_idtr(IDTR *idtr) {
    __asm__ __volatile__("lidt (%0)" : : "r"(idtr));
}

// Execute `sidt` to load IDTR from a variable
static inline void cpu_store_idtr(IDTR *idtr) {
    __asm__ __volatile__("sidt (%0)" : : "r"(idtr));
}

static inline void cpu_invalidate_page(uintptr_t virt_addr) {
    __asm__ volatile("invlpg (%0)\n\t" : : "r"(virt_addr) : "memory");
}

static inline void cpu_swapgs(void) {
#ifndef NO_USER_GS
    __asm__ volatile("swapgs" : : : "memory");
#endif
}

#endif //__ANOS_KERNEL_DRIVERS_CPU_H