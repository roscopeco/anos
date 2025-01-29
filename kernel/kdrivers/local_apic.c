/*
 * stage3 - Local APIC Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "acpitables.h"
#include "debugprint.h"
#include "kdrivers/drivers.h"
#include "kdrivers/hpet.h"
#include "kdrivers/local_apic.h"
#include "kdrivers/timer.h"
#include "machine.h"
#include "printdec.h"
#include "printhex.h"
#include "spinlock.h"
#include "vmm/vmmapper.h"

#define NANOS_IN_20MS (((uint64_t)20000000))

static void start_timer(uint32_t volatile *lapic, uint8_t mode,
                        uint32_t init_count) {
    // Set up timer
    *REG_LAPIC_DIVIDE(lapic) = mode;
    *REG_LAPIC_INITIAL_COUNT(lapic) = init_count;
    *REG_LAPIC_LVT_TIMER(lapic) = 0x20000 | LAPIC_TIMER_VECTOR;
}

static uint64_t local_apic_calibrate_count(KernelTimer *calibrated_timer,
                                           uint32_t desired_hz) {
    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);

    uint64_t calibrated_ticks_20ms =
            NANOS_IN_20MS / calibrated_timer->nanos_per_tick();

    volatile uint64_t calib_start = calibrated_timer->current_ticks();
    uint64_t calib_end = calib_start + calibrated_ticks_20ms;

    *REG_LAPIC_DIVIDE(lapic) = 0x03;
    *REG_LAPIC_INITIAL_COUNT(lapic) = 0xffffffff;
    *REG_LAPIC_LVT_TIMER(lapic) = 0x20000 | LAPIC_TIMER_VECTOR;

    while (calib_start < calib_end) {
        calib_start = calibrated_timer->current_ticks();
    }

    *REG_LAPIC_LVT_TIMER(lapic) = 0x10000 | LAPIC_TIMER_VECTOR;

    uint64_t ticks_in_20ms = 0xffffffff - *REG_LAPIC_CURRENT_COUNT(lapic);

#ifdef DEBUG_CPU
#ifdef DEBUG_CPU_FREQ
    debugstr("Calibrated ");
    printdec(ticks_in_20ms, debugchar);
    debugstr(" LAPIC ticks in 20ms...");
#endif
#endif

    return ticks_in_20ms * 50 / desired_hz;
}

static SpinLock init_timers_spinlock;

uint32_t volatile *init_local_apic(ACPI_MADT *madt, bool bsp) {
    uint32_t lapic_addr = madt->lapic_address;
#ifdef DEBUG_LAPIC_INIT
    uint32_t *flags = lapic_addr + 1;
    debugstr("LAPIC address (phys : virt) = ");
    printhex32(*lapic_addr, debugchar);
    debugstr(" : 0xffffffa000000000 [");
    printhex32(*flags, debugchar);
    debugstr("]\n");
#endif

    if (bsp) {
        vmm_map_page(KERNEL_HARDWARE_VADDR_BASE, lapic_addr, PRESENT | WRITE);
    }

    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);

#ifdef DEBUG_LAPIC_INIT
    debugstr("LAPIC ID: ");
    printhex32(*REG_LAPIC_ID(lapic), debugchar);
    debugstr("; Version: ");
    printhex32(*REG_LAPIC_VERSION(lapic), debugchar);
    debugstr("\n");
#endif

    // Set spurious interrupt and enable
    *(REG_LAPIC_SPURIOUS(lapic)) = 0x1FF;

    // if (bsp) {
    spinlock_lock(&init_timers_spinlock);
    KernelTimer *timer = hpet_as_timer();

    uint64_t hz_ticks;
    hz_ticks = local_apic_calibrate_count(timer, KERNEL_HZ);
    spinlock_unlock(&init_timers_spinlock);

    // /16 mode, init count based on caibrated kernel Hz.
    if (bsp) {
        // Can't start AP timer ticks yet, we don't have everything set up
        // to handle them...
        start_timer(lapic, 0x03, hz_ticks);
    }

    return lapic;
}

uint64_t local_apic_get_count(void) {
    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);
    return *REG_LAPIC_CURRENT_COUNT(lapic);
}

void local_apic_eoe(void) {
    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);
    *(REG_LAPIC_EOI(lapic)) = 0;
}
