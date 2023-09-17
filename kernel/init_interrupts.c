/*
 * stage3 - Install trap / interrupt handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>
#include "interrupts.h"

// This is a bit messy, but it works and is "good enough" for now 😅
#define install_trap(N) do {                                                                    \
        extern void (isr_dispatcher_##N)(void);                                                 \
        idt_entry(idt + N, isr_dispatcher_##N, kernel_cs, 0, idt_attr(1, 0, IDT_TYPE_TRAP));    \
    } while (0)

extern void irq_handler(void);

// These can't live here long-term, but it'll do for now...
static IdtEntry idt[256];
static Idtr     idtr;

void idt_install(uint16_t kernel_cs) {
    install_trap(0);
    install_trap(1);
    install_trap(2);
    install_trap(3);
    install_trap(4);
    install_trap(5);
    install_trap(6);
    install_trap(7);
    install_trap(8);
    install_trap(9);
    install_trap(10);
    install_trap(11);
    install_trap(12);
    install_trap(13);
    install_trap(14);
    install_trap(15);
    install_trap(16);
    install_trap(17);
    install_trap(18);
    install_trap(19);
    install_trap(20);
    install_trap(21);
    install_trap(22);
    install_trap(23);
    install_trap(24);
    install_trap(25);
    install_trap(26);
    install_trap(27);
    install_trap(28);
    install_trap(29);
    install_trap(30);
    install_trap(31);

    // Just fill the rest of the table with "not present" handlers for now...
    for (int i = 0x20; i < 0x100; i++) {
        idt_entry(idt + i, irq_handler, kernel_cs, 0, idt_attr(0, 0, IDT_TYPE_IRQ));
    }

    // Setup the IDTR
    idt_r(&idtr, (uint64_t)idt, (uint16_t)sizeof(IdtEntry) * 256 - 1);

    // And load it!
    __asm__ volatile (
        "lidt %0" : : "m"(idtr)
    );
}
