/*
 * stage3 - LAPIC timer ISR (heartbeat)
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "kdrivers/local_apic.h"
#include "sched.h"
#include "sleep.h"

// TODO Obviously doesn't belong here, just a hack for proof of life...
#define VRAM_VIRTUAL_HEART 0xffffffff800b809e
static bool heart_state = false;
static volatile uint32_t counter;

// TODO could probably derive this directly from timer count now...?
volatile uint64_t lapic_timer_upticks;

uint64_t get_lapic_timer_upticks(void) { return lapic_timer_upticks; }

void handle_ap_timer_interrupt(void) {
    local_apic_eoe();

    sched_lock();
    check_sleepers();
    sched_schedule();
    sched_unlock();
}

void handle_bsp_timer_interrupt(void) {
    lapic_timer_upticks += 1;

    if (counter++ == (KERNEL_HZ / 2)) /* one full cycle / sec */ {
        counter = 0;

        uint8_t *vram = (uint8_t *)VRAM_VIRTUAL_HEART;

        vram[0] = 0x03; // heart

        if (heart_state) {
            vram[1] = 0x0C; // red
        } else {
            vram[1] = 0x08; // "light black"
        }

        heart_state = !heart_state;
    }

    local_apic_eoe();

    sched_lock();
    check_sleepers();
    sched_schedule();
    sched_unlock();
}