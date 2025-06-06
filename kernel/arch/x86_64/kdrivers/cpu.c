/*
 * stage3 - x86_64 CPU Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "debugprint.h"
#include "platform/acpi/acpitables.h"
#include "x86_64/cpuid.h"
#include "x86_64/kdrivers/cpu.h"

#ifdef DEBUG_CPU
#include "kprintf.h"
#endif

// 0 = not checked, -1 = don't have, 1 = have
static int __have__cpu__rdseed;

bool cpu_init_this(void) {
    init_cpuid();

    return true;
}

uint64_t cpu_read_local_apic_id(void) {
    uint8_t bsp_apic_id;
    __asm__ __volatile__("mov $1, %%eax\n\t"
                         "cpuid\n\t"
                         "shrl $24, %%ebx\n\t"
                         : "=b"(bsp_apic_id)
                         :
                         :);
    return bsp_apic_id;
}

inline void cpu_tsc_delay(uint64_t cycles) {
    uint64_t wake_at = cpu_read_tsc() + cycles;
    while (cpu_read_tsc() < wake_at)
        ;
}

void cpu_tsc_mdelay(int n) {
    // TODO actually calibrate?
    cpu_tsc_delay(n * 10000000);
}

void cpu_tsc_udelay(int n) {
    // TODO actually calibrate?
    cpu_tsc_delay(n * 1000);
}

__attribute__((no_sanitize("alignment")))   // CPUID forces us to be unaligned here...
void cpu_get_brand_str(char *buffer) {
    uint32_t *buf_ptr = (uint32_t *)buffer;

    for (int i = 0; i < 49; i++) {
        buffer[i] = 0;
    }

    for (uint64_t leaf = 0x80000002; leaf < 0x80000005; leaf++) {
        uint32_t eax, ebx, ecx, edx;
        if (cpuid(leaf, &eax, &ebx, &ecx, &edx)) {
            *buf_ptr++ = eax;
            *buf_ptr++ = ebx;
            *buf_ptr++ = ecx;
            *buf_ptr++ = edx;
        }
    }
}

#ifdef DEBUG_CPU
static void debug_cpu_brand(uint8_t cpu_num) {
    char brand[49];
    cpu_get_brand_str(brand);
    kprintf("CPU #%2d: %s\n", cpu_num, brand);
}
#else
#define debug_cpu_brand(...)
#endif

#ifdef DEBUG_CPU_FREQ
static void debug_tsc_frequency_cpuid(void) {
    uint32_t tsc_denominator, tsc_numerator, cpu_crystal_hz, edx;
    if (cpuid(0x15, &tsc_denominator, &tsc_numerator, &cpu_crystal_hz, &edx)) {
        if (tsc_denominator & tsc_numerator & cpu_crystal_hz) {
            uint64_t cpu_hz =
                    (cpu_crystal_hz * tsc_numerator) / tsc_denominator;
            kprintf("TSC frequency (CPUID): %ldHz\n", cpu_hz);
        } else {
            kprintf("TSC frequency (CPUID): <unspecified>\n");
        }
    } else {
        kprintf("TSC frequency (CPUID): <unknown>\n");
    }
}

static void debug_tsc_frequency_msr(void) {
    uint64_t tsc_base = ((cpu_read_msr(0xce) & 0xff00) >> 8);

    if (tsc_base > 0) {
        kprintf("TSC frequency (MSR)  : %ldHz\n", tsc_base * 100000);
    } else {
        kprintf("TSC frequency (MSR)  : <unknown>\n");
    }
}
#else
#define debug_tsc_frequency_cpuid()
#define debug_tsc_frequency_msr()
#endif

void cpu_debug_info(uint8_t cpu_num) {
    debug_cpu_brand(cpu_num);
    debug_tsc_frequency_cpuid();
    debug_tsc_frequency_msr();
}

uint64_t cpu_read_msr(uint32_t msr) {
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr) : "memory");
    return ((uint64_t)edx << 32) | eax;
}

void cpu_write_msr(uint64_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

uint64_t cpu_read_tsc(void) {
    uint32_t edx, eax;
    __asm__ volatile("rdtsc" : "=a"(eax), "=d"(edx) : : "memory");
    return ((uint64_t)edx << 32) | eax;
}

// Execute `lgdt` to load a variable with the GDTR
void cpu_load_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("lgdt (%0)" : : "r"(gdtr));
}

// Execute `sgdt` to load GDTR from a variable
void cpu_store_gdtr(GDTR *gdtr) {
    __asm__ __volatile__("sgdt (%0)" : : "r"(gdtr) : "memory");
}

// Execute `lidt` to load a variable with the IDTR
void cpu_load_idtr(IDTR *idtr) {
    __asm__ __volatile__("lidt (%0)" : : "r"(idtr));
}

// Execute `sidt` to load IDTR from a variable
void cpu_store_idtr(IDTR *idtr) {
    __asm__ __volatile__("sidt (%0)" : : "r"(idtr));
}

void cpu_invalidate_tlb_addr(uintptr_t virt_addr) {
    __asm__ volatile("invlpg (%0)\n\t" : : "r"(virt_addr) : "memory");
}

void cpu_invalidate_tlb_all(void) {
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0\n\t"
                     "mov %0, %%cr3"
                     : "=r"(cr3)::"memory");
}
void cpu_swapgs(void) {
#ifndef NO_USER_GS
    __asm__ volatile("swapgs" : : : "memory");
#endif
}

uintptr_t cpu_read_cr3(void) {
    uintptr_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}
// 0 = not checked, -1 = don't have, 1 = have
static int __have__cpu__rdseed;

bool cpu_rdseed64(uint64_t *value) {
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

bool cpu_rdseed32(uint32_t *value) {
    unsigned char ok;
    __asm__ volatile("rdseed %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

bool cpu_rdrand64(uint64_t *value) {
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}

bool cpu_rdrand32(uint32_t *value) {
    unsigned char ok;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*value), "=qm"(ok) : : "cc");
    return ok;
}
