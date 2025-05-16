/*
 * stage3 - Interrupt / IDT support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_INTERRUPTS_H
#define __ANOS_KERNEL_INTERRUPTS_H

#include <stdint.h>

typedef struct {
    uint16_t isr_low;
    uint16_t segment;
    uint8_t ist_entry;
    uint8_t attr;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed)) IdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTR;

typedef void(isr_dispatcher)(void);

#define IDT_TYPE_IRQ 0x0E
#define IDT_TYPE_TRAP 0x0F

#define idt_attr(present, dpl, type)                                           \
    (((((uint8_t)present) > 0 ? 0x80 : 0x00) | ((uint8_t)((dpl & 0x3) << 5)) | \
      (((uint8_t)type) & 0xF)))

void idt_entry(IdtEntry *target, isr_dispatcher *handler, uint16_t segment,
               uint8_t ist_entry, uint8_t attr);
void idt_r(IDTR *target, uintptr_t base, uint16_t limit);

void idt_install(uint16_t kernel_cs);

void idt_install_isr(uint8_t vector, isr_dispatcher *dispatcher,
                     uint8_t ist_entry, uint8_t dpl, uint8_t handler_type,
                     uint8_t present);

#endif
