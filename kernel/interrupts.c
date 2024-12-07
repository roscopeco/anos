/*
 * stage3 - Interrupt / IDT support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "interrupts.h"
#include <stdint.h>

void idt_entry(IdtEntry *target, isr_dispatcher *handler, uint16_t segment,
               uint8_t ist_entry, uint8_t attr) {
    target->isr_low = ((uintptr_t)handler) & 0xFFFF;
    target->isr_mid = (((uintptr_t)handler) & 0xFFFF0000) >> 16;
    target->isr_high = (((uintptr_t)handler) & 0xFFFFFFFF00000000) >> 32;
    target->segment = segment;
    target->ist_entry = ist_entry;
    target->attr = attr;
}

void idt_r(Idtr *target, uintptr_t base, uint16_t limit) {
    target->limit = limit;
    target->base = base;
}
