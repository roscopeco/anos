/*
 * stage3 - LAPIC timer ISR (heartbeat)
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "kdrivers/local_apic.h"

// TODO Obviously doesn't belong here, just a hack for proof of life...
#define VRAM_VIRTUAL_HEART 0xffffffff800b809e
static bool heart_state = false;
void handle_timer_interrupt() {
    uint8_t *vram = (uint8_t *)VRAM_VIRTUAL_HEART;

    vram[0] = 0x03; // heart

    if (heart_state) {
        vram[1] = 0x0C; // red
    } else {
        vram[1] = 0x08; // "light black"
    }

    heart_state = !heart_state;
    local_apic_eoe();
}