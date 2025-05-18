/*
 * stage3 - Install trap / interrupt handlers
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include <stdint.h>

#include "interrupts.h"
#include "smp/ipwi.h"
#include "syscalls.h"
#include "x86_64/kdrivers/local_apic.h" // TODO this shouldn't be used here...

// This is a bit messy, but it works and is "good enough" for now 😅
#define install_trap(N)                                                        \
    do {                                                                       \
        extern void(trap_dispatcher_##N)(void);                                \
        idt_entry(idt + N, trap_dispatcher_##N, kernel_cs, 0,                  \
                  idt_attr(1, 0, IDT_TYPE_TRAP));                              \
    } while (0)

extern void pic_irq_handler(void);
extern void bsp_timer_interrupt_handler(void);
extern void ap_timer_interrupt_handler(void);
extern void unknown_interrupt_handler(void);
extern void syscall_69_handler(void);
extern void ipwi_ipi_dispatcher(void);

extern void pic_init(void);

// These can't live here long-term, but it'll do for now...
static IdtEntry idt[256];
static IDTR idtr;
static uint16_t saved_kernel_cs;

void idt_install(uint16_t kernel_cs) {
    saved_kernel_cs = kernel_cs;

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

    // Entries 0x20 - 0x2F are the PIC handlers - when disabled, they should
    // only ever be spurious (except NMI I suppose)...
    for (int i = 0x20; i < 0x30; i++) {
        idt_entry(idt + i, pic_irq_handler, kernel_cs, 0,
                  idt_attr(1, 0, IDT_TYPE_IRQ));
    }

    // Just fill the rest of the table with generic / unknown handlers for now,
    // Use IRQ type since these don't return and that'll disable interrupts for
    // us...
    for (int i = 0x30; i < 0x100; i++) {
        idt_entry(idt + i, unknown_interrupt_handler, kernel_cs, 0,
                  idt_attr(1, 0, IDT_TYPE_IRQ));
    }

    // Set up the handlers for the LAPIC Timer vectors...
    idt_entry(idt + LAPIC_TIMER_BSP_VECTOR, bsp_timer_interrupt_handler,
              kernel_cs, 0, idt_attr(1, 0, IDT_TYPE_IRQ));
    idt_entry(idt + LAPIC_TIMER_AP_VECTOR, ap_timer_interrupt_handler,
              kernel_cs, 0, idt_attr(1, 0, IDT_TYPE_IRQ));

    // Set up the handler for the 0x69 syscall...
    idt_entry(idt + SYSCALL_VECTOR, syscall_69_handler, kernel_cs, 0,
              idt_attr(1, 3, IDT_TYPE_TRAP));

    // Set up the handlers for kernel IPIs
    idt_entry(idt + IPWI_IPI_VECTOR, ipwi_ipi_dispatcher, kernel_cs, 0,
              idt_attr(1, 0, IDT_TYPE_IRQ));

    // Setup the IDTR
    idt_r(&idtr, (uint64_t)idt, (uint16_t)sizeof(IdtEntry) * 256 - 1);

    // Init (i.e. disable) the PICs
    pic_init();

    // And load it!
    __asm__ volatile("lidt %0" : : "m"(idtr));

    // Enable interrupts
    __asm__ volatile("sti");
}

void idt_install_isr(const uint8_t vector, isr_dispatcher *dispatcher,
                     const uint8_t ist_entry, const uint8_t dpl,
                     const uint8_t handler_type, const uint8_t present) {
    idt_entry(idt + vector, dispatcher, saved_kernel_cs, ist_entry,
              idt_attr(present ? 1 : 0, dpl, handler_type));
}
