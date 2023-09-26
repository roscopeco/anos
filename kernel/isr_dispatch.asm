; stage3 - Kernel ISR dispatchers (to C handlers)
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; ISR dispatchers for calling C code in ISRs
;

bits 64
global pic_irq_handler, irq_handler, spurious_irq_count
extern handle_interrupt_nc, handle_interrupt_wc

%macro isr_dispatcher_with_code 1         ; General handler for traps that stack an error code
global isr_dispatcher_%+%1                ; Ensure declared global
isr_dispatcher_%+%1:                      ; Name e.g. `isr_dispatcher_0`
  push  rax                               ; Save all C-clobbered registers
  push  rcx
  push  rdx
  push  rsi
  push  rdi
  push  r8
  push  r9
  push  r10
  push  r11  
  
  mov   rdi,%1                            ; Put the trap number in the first C argument
  mov   rsi,72[rsp]                       ; Error code from stack into the second C argument
  mov   rdx,80[rsp]                       ; Peek return address into third C argument
  call  handle_interrupt_wc               ; Call the with-code handler

  pop   r11                               ; Restore registers...
  pop   r10
  pop   r9
  pop   r8

  pop   rdi
  pop   rsi
  pop   rdx
  pop   rcx
  pop   rax
  add   rsp,8                             ; Discard the error code

  iretq                                    ; And done...
%endmacro

%macro isr_dispatcher_no_code 1           ; General handler for traps that stack an error code
global isr_dispatcher_%+%1                ; Ensure declared global
isr_dispatcher_%+%1:                      ; Name e.g. `isr_dispatcher_1`
  push  rax                               ; Save all C-clobbered registers
  push  rcx
  push  rdx
  push  rsi
  push  rdi
  push  r8
  push  r9
  push  r10
  push  r11  
  
  mov   rdi,%1                            ; Put the trap number in the first C argument (TODO changing registers!)
  mov   rsi,72[rsp]                       ; Peek return address into second C argument
  call  handle_interrupt_nc               ; Call the no-code handler

  pop   r11                              ; Restore registers...
  pop   r10
  pop   r9
  pop   r8
  pop   rdi
  pop   rsi
  pop   rdx
  pop   rcx
  pop   rax
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

; ISR dispatcher for PIC IRQs (all of which _should_ be spurious)
pic_irq_handler:
  push  rax                               ; Stash rax
  mov   rax,[spurious_irq_count]          ; Load the count
  inc   rax                               ; Increment it
  mov   [spurious_irq_count],rax          ; And store it back
  pop   rax                               ; Restore regs
  iretq                                   ; And done

; ISR dispatcher for IRQs
irq_handler:
  call  handle_interrupt_nc               ; Just call directly to C handler
  iretq

; Running count of spurious IRQs
spurious_irq_count  dq  0
