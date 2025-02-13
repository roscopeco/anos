/*
 * stage3 - x86_64 CPU Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>

#include "acpitables.h"
#include "cpuid.h"
#include "debugprint.h"
#include "kdrivers/cpu.h"
#include "printdec.h"
#include "printhex.h"

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

    debugstr("CPU #");
    printdec(cpu_num, debugchar);
    debugstr(": ");
    debugstr(&brand[0]);
    debugstr("\n");
}
#else
#define debug_cpu_brand(...)
#endif

#ifdef DEBUG_CPU_FREQ
static void debug_tsc_frequency_cpuid(void) {
    debugstr("TSC frequency (CPUID): ");

    uint32_t tsc_denominator, tsc_numerator, cpu_crystal_hz, edx;
    if (cpuid(0x15, &tsc_denominator, &tsc_numerator, &cpu_crystal_hz, &edx)) {
        if (tsc_denominator & tsc_numerator & cpu_crystal_hz) {
            uint64_t cpu_hz =
                    (cpu_crystal_hz * tsc_numerator) / tsc_denominator;
            printdec(cpu_hz, debugchar);
            debugstr("Hz");
        } else {
            debugstr("<unspecified>");
        }
    } else {
        debugstr("<unknown>");
    }

    debugstr("\n");
}

static void debug_tsc_frequency_msr(void) {
    debugstr("TSC frequency (MSR)  : ");

    uint64_t tsc_base = ((cpu_read_msr(0xce) & 0xff00) >> 8);

    if (tsc_base > 0) {
        printdec(tsc_base * 100000, debugchar);
        debugstr("Hz");
    } else {
        debugstr("<unknown>");
    }

    debugstr("\n");
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
