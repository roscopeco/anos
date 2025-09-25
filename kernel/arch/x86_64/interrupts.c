/*
 * stage3 - Interrupt / IDT support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "x86_64/interrupts.h"
#include <stdint.h>

void idt_entry(IdtEntry *target, isr_dispatcher *handler, const uint16_t segment, const uint8_t ist_entry,
               const uint8_t attr) {
    target->isr_low = ((uintptr_t)handler) & 0xFFFF;
    target->isr_mid = (((uintptr_t)handler) & 0xFFFF0000) >> 16;
    target->isr_high = (((uintptr_t)handler) & 0xFFFFFFFF00000000) >> 32;
    target->segment = segment;
    target->ist_entry = ist_entry;
    target->attr = attr;
}

void idt_r(IDTR *target, const uintptr_t base, const uint16_t limit) {
    target->limit = limit;
    target->base = base;
}
