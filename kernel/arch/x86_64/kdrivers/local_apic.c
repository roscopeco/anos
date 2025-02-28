/*
 * stage3 - Local APIC Kernel driver
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "acpitables.h"
#include "kdrivers/drivers.h"
#include "kdrivers/hpet.h"
#include "kdrivers/local_apic.h"
#include "kdrivers/timer.h"
#include "kprintf.h"
#include "machine.h"
#include "spinlock.h"
#include "vmm/vmmapper.h"

#define NANOS_IN_20MS (((uint64_t)20000000))

static void start_timer(uint32_t volatile *lapic, uint8_t mode,
                        uint32_t init_count, uint8_t vector) {
    // Set up timer
    *REG_LAPIC_DIVIDE(lapic) = mode;
    *REG_LAPIC_INITIAL_COUNT(lapic) = init_count;
    *REG_LAPIC_LVT_TIMER(lapic) = 0x20000 | vector;
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
    *REG_LAPIC_LVT_TIMER(lapic) = 0x20000 | LAPIC_TIMER_BSP_VECTOR;

    while (calib_start < calib_end) {
        calib_start = calibrated_timer->current_ticks();
    }

    *REG_LAPIC_LVT_TIMER(lapic) = 0x10000 | LAPIC_TIMER_BSP_VECTOR;

    uint64_t ticks_in_20ms = 0xffffffff - *REG_LAPIC_CURRENT_COUNT(lapic);

#ifdef DEBUG_CPU
#ifdef DEBUG_CPU_FREQ
    kprintf("Calibrated %ld LAPIC ticks in 20ms...\n", ticks_in_20ms);
#endif
#endif

    return ticks_in_20ms * 50 / desired_hz;
}

static SpinLock init_timers_spinlock;

uint32_t volatile *init_local_apic(ACPI_MADT *madt, bool bsp) {
    uint32_t lapic_addr = madt->lapic_address;
#ifdef DEBUG_LAPIC_INIT
    uint32_t *flags = (uint32_t *)((uintptr_t)lapic_addr) + 1;
    kprintf("LAPIC address (phys : virt) = 0x%08x : 0xffffffa000000000\n",
            lapic_addr);
#endif

    if (bsp) {
        vmm_map_page(KERNEL_HARDWARE_VADDR_BASE, lapic_addr, PRESENT | WRITE);
    }

    uint32_t volatile *lapic = (uint32_t *)(KERNEL_HARDWARE_VADDR_BASE);

#ifdef DEBUG_LAPIC_INIT
    kprintf("LAPIC ID: 0x%08x; Version: 0x%08x\n", *REG_LAPIC_ID(lapic),
            *REG_LAPIC_VERSION(lapic));
#endif

    // Set spurious interrupt and enable
    *(REG_LAPIC_SPURIOUS(lapic)) = 0x1FF;

    // if (bsp) {
    uint64_t lock_flags = spinlock_lock_irqsave(&init_timers_spinlock);
    KernelTimer *timer = hpet_as_timer();

    uint64_t hz_ticks;
    hz_ticks = local_apic_calibrate_count(timer, KERNEL_HZ);
    spinlock_unlock_irqrestore(&init_timers_spinlock, lock_flags);

    // /16 mode, init count based on caibrated kernel Hz.
    if (bsp) {
        // Can't start AP timer ticks yet, we don't have everything set up
        // to handle them...
        start_timer(lapic, 0x03, hz_ticks, LAPIC_TIMER_BSP_VECTOR);
    } else {
        start_timer(lapic, 0x03, hz_ticks, LAPIC_TIMER_AP_VECTOR);
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
