; stage3 - IPWI interrupt dispatcher
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

bits 64

%include "kernel/arch/x86_64/isr_common.asm"

global ipwi_ipi_dispatcher
extern ipwi_ipi_handler

; Register this with IRQ_TYPE_IRQ to disable interrupts (may not
; matter now we use NMI...)
;
ipwi_ipi_dispatcher:
    cld
    irq_conditional_swapgs
    pusha_sysv                              ; Push all caller-saved registers

    call ipwi_ipi_handler

    popa_sysv                               ; Restore all caller-saved registers
    irq_conditional_swapgs
    iretq