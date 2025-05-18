; stage3 - IPWI interrupt dispatcher
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

global ipwi_ipi_dispatcher
extern ipwi_ipi_handler

; Register this with IRQ_TYPE_IRQ to disable interrupts (may not
; matter now we use NMI...)
;
ipwi_ipi_dispatcher:
    call ipwi_ipi_handler
    iretq
