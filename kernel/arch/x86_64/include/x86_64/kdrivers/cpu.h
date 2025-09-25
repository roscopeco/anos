/*
 * stage3 - Kernel CPU driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_ARCH_X86_64_DRIVERS_CPU_H
#define __ANOS_KERNEL_ARCH_X86_64_DRIVERS_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "anos_assert.h"
#include "x86_64/cpuid.h"
#include "x86_64/gdt.h"
#include "x86_64/interrupts.h"

#define MSR_FSBase ((0xC0000100))
#define MSR_GSBase ((0xC0000101))
#define MSR_KernelGSBase ((0xC0000102))
#define MSR_IA32_PAT ((0x277))

// PAT Memory Types
#define PAT_UNCACHEABLE ((0x00))
#define PAT_WRITE_COMBINING ((0x01))
#define PAT_WRITE_THROUGH ((0x04))
#define PAT_WRITE_PROTECTED ((0x05))
#define PAT_WRITE_BACK ((0x06))
#define PAT_UNCACHED_MINUS ((0x07))

#ifndef MAX_CPU_COUNT
#ifndef NO_SMP
#define MAX_CPU_COUNT ((16))
#else
#define MAX_CPU_COUNT ((1))
#endif
#endif

static_assert(MAX_CPU_COUNT > 0, "Cannot build a kernel for zero CPUs!");

#define CPU_TSS_ENTRY_SIZE_MULT ((2))

bool cpu_init_this(void);

uint64_t cpu_read_local_apic_id(void);

void cpu_tsc_delay(uint64_t cycles);
void cpu_tsc_mdelay(int n);
void cpu_tsc_udelay(int n);

// Buffer must have space for 49 characters!
void cpu_get_brand_str(char *buffer);

void cpu_debug_info(uint8_t cpu_num);

uint64_t cpu_read_msr(uint32_t msr);

void cpu_write_msr(uint64_t msr, uint64_t value);

void cpu_write_pat(uint8_t pat0, uint8_t pat1, uint8_t pat2, uint8_t pat3, uint8_t pat4, uint8_t pat5, uint8_t pat6,
                   uint8_t pat7);

uint64_t cpu_read_tsc(void);

// Execute `lgdt` to load a variable with the GDTR
void cpu_load_gdtr(GDTR *gdtr);

// Execute `sgdt` to load GDTR from a variable
void cpu_store_gdtr(GDTR *gdtr);

// Execute `lidt` to load a variable with the IDTR
void cpu_load_idtr(IDTR *idtr);

// Execute `sidt` to load IDTR from a variable
void cpu_store_idtr(IDTR *idtr);

void cpu_invalidate_tlb_addr(uintptr_t virt_addr);

void cpu_invalidate_tlb_all(void);

void cpu_swapgs(void);

static inline uintptr_t cpu_make_pagetable_register_value(uintptr_t pt_base) { return pt_base; }

uintptr_t cpu_read_cr3(void);

#define cpu_get_pagetable_root_phys (cpu_read_cr3())

bool cpu_rdseed64(uint64_t *value);

bool cpu_rdseed32(uint32_t *value);

bool cpu_rdrand64(uint64_t *value);

bool cpu_rdrand32(uint32_t *value);

#endif //__ANOS_KERNEL_ARCH_X86_64_DRIVERS_CPU_H