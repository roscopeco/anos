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
#include "interrupts.h"
#include "x86_64/cpuid.h"
#include "x86_64/gdt.h"

#define MSR_FSBase ((0xC0000100))
#define MSR_GSBase ((0xC0000101))
#define MSR_KernelGSBase ((0xC0000102))

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

// 0 = not checked, -1 = don't have, 1 = have
static int __have__cpu__rdseed;

static inline bool cpu_rdseed64(uint64_t *value) {
    // TODO we should probably just check this statically at startup...
    //
    // There's also a potentially weird race here, but only if the
    // system is multi-processor but not symmetric, and we get
    // somehow scheduled onto another (older, but still x86_64)
    // CPU during this func - in theory there could be a #UD.
    //
    // I don't care about this, because in those circumstances I
    // think this will be the least of my worries...
    //
    if (__have__cpu__rdseed < 1) {
        if (__have__cpu__rdseed == -1) {
            // already know we don't have it...
            return false;
        }

        // otherwise, check it
        uint32_t eax, ebx, ecx, edx;
        if (cpuid(0x7, &eax, &ebx, &ecx, &edx)) {
            if (ebx & (1 << 18)) {
                // we have it (we're on broadwell or later)
                __have__cpu__rdseed = 1;
            } else {
                // we don't have it (we're on haswell)
                __have__cpu__rdseed = -1;
                return false;
            }
        } else {
            // still don't know, we'll check again next time...
            return false;
        }
    }

    unsigned char ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

static inline bool cpu_rdseed32(uint32_t *value) {
    unsigned char ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

static inline bool cpu_rdrand64(uint64_t *value) {
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

static inline bool cpu_rdrand32(uint32_t *value) {
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

#endif //__ANOS_KERNEL_ARCH_X86_64_DRIVERS_CPU_H