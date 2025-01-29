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

bool cpu_init_this(void);

uint64_t cpu_read_msr(uint32_t msr);
uint64_t cpu_read_tsc(void);
uint64_t cpu_read_local_apic_id(void);

void cpu_tsc_delay(uint64_t cycles);
void cpu_tsc_mdelay(int n);
void cpu_tsc_udelay(int n);

void cpu_debug_info(void);

// Execute `lgdt` to load a variable with the GDTR
static inline void cpu_load_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("lgdt (%0)" : : "r"(gdtr));
}

// Execute `sgdt` to load GDTR from a variable
static inline void cpu_store_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("sgdt (%0)" : : "r"(gdtr));
}

// Execute `lidt` to load a variable with the IDTR
static inline void cpu_load_idtr(IDTR *idtr) {
    __asm__ __volatile__("lidt (%0)" : : "r"(idtr));
}

// Execute `sidt` to load IDTR from a variable
static inline void cpu_store_idtr(IDTR *idtr) {
    __asm__ __volatile__("sidt (%0)" : : "r"(idtr));
}

#endif //__ANOS_KERNEL_DRIVERS_CPU_H