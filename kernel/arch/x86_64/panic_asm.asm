; stage3 - Panic functions implemented in assembly
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

global panic_ipi_handler

; Register this with IRQ_TYPE_TRAP to disable interrupts...
panic_ipi_handler:
    hlt
    jmp panic_ipi_handler
