; stage3 - Kernel ISR dispatchers (to C handlers)
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; ISR dispatchers for calling C code in ISRs
;

bits 64
global irq_handler
extern debug_interrupt_nc, debug_interrupt_wc

%macro isr_dispatcher_with_code 1         ; General handler for traps that stack an error code
global isr_dispatcher_%+%1                ; Ensure declared global
isr_dispatcher_%+%1:                      ; Name e.g. `isr_dispatcher_0`
  mov   rdi,%1                            ; Put the trap number in the first C argument (TODO changing registers!)
  pop   rsi                               ; Pop error code into the second C argument
  call  debug_interrupt_wc                ; Call the with-code handler
  iretq                                   ; And done...
%endmacro

%macro isr_dispatcher_no_code 1           ; General handler for traps that stack an error code
global isr_dispatcher_%+%1                ; Ensure declared global
isr_dispatcher_%+%1:                      ; Name e.g. `isr_dispatcher_1`
  mov   rdi,%1                            ; Put the trap number in the first C argument (TODO changing registers!)
  call  debug_interrupt_nc                ; Call the no-code handler
  iretq                                   ; And done...
%endmacro

isr_dispatcher_no_code      0             ; Division Error
isr_dispatcher_no_code      1             ; Debug
isr_dispatcher_no_code      2             ; NMI
isr_dispatcher_no_code      3             ; Breakpoint
isr_dispatcher_no_code      4             ; Overflow
isr_dispatcher_no_code      5             ; Bound Range Exceeded
isr_dispatcher_no_code      6             ; Invalid Opcode
isr_dispatcher_no_code      7             ; Device Not Available
isr_dispatcher_with_code    8             ; Double Fault
isr_dispatcher_no_code      9             ; Coprocessor Segment Overrun
isr_dispatcher_with_code    10            ; Invalid TSS
isr_dispatcher_with_code    11            ; Segment Not Present
isr_dispatcher_with_code    12            ; Stack-segment Fault
isr_dispatcher_with_code    13            ; GPF
isr_dispatcher_with_code    14            ; Page Fault
isr_dispatcher_no_code      15            ; <reserved>
isr_dispatcher_no_code      16            ; x87 Floating-point Exception
isr_dispatcher_with_code    17            ; Alignment Check
isr_dispatcher_no_code      18            ; Machine Check
isr_dispatcher_no_code      19            ; SIMD Floating-point Exception
isr_dispatcher_no_code      20            ; Virtualization Exception
isr_dispatcher_no_code      21            ; Control Protection Exception
isr_dispatcher_no_code      22            ; <reserved>
isr_dispatcher_no_code      23            ; <reserved> 
isr_dispatcher_no_code      24            ; <reserved>
isr_dispatcher_no_code      25            ; <reserved>
isr_dispatcher_no_code      26            ; <reserved>
isr_dispatcher_no_code      27            ; <reserved>
isr_dispatcher_no_code      28            ; Hypervisor Injection Exception
isr_dispatcher_no_code      29            ; VMM Communication Exception
isr_dispatcher_with_code    30            ; Security Exception
isr_dispatcher_no_code      31            ; <reserved>

; ISR dispatcher for IRQs
irq_handler:
  call  debug_interrupt_nc                ; Just call directly to C handler
  iretq
