/*
 * stage3 - The page fault handler for RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#include "x86_64/interrupts.h"

static constexpr uint8_t PAGEFAULT_VECTOR = 0x0e;

// isr_dispatch.asm
void page_fault_dispatcher(void);

void pagefault_notify_smp_started(void) {
    idt_install_isr(PAGEFAULT_VECTOR, page_fault_dispatcher, 0, 0,
                    IDT_TYPE_TRAP, 1);
}