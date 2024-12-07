; stage3 - Kernel ISR dispatchers (to C handlers)
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; ISR dispatchers for calling C code in ISRs
;

bits 64
global pic_irq_handler, timer_interrupt_handler, unknown_interrupt_handler, spurious_irq_count
extern handle_exception_nc, handle_exception_wc, handle_timer_interrupt, handle_unknown_interrupt

%macro pusha_sysv 0
  push  rax                               ; Save all C-clobbered registers
  push  rcx
  push  rdx
  push  rsi
  push  rdi
  push  r8
  push  r9
  push  r10
  push  r11  
%endmacro

%macro popa_sysv 0
  pop   r11                               ; Restore registers...
  pop   r10
  pop   r9
  pop   r8
  pop   rdi
  pop   rsi
  pop   rdx
  pop   rcx
  pop   rax
%endmacro

%macro trap_dispatcher_with_code 1        ; General handler for traps that stack an error code
global trap_dispatcher_%+%1               ; Ensure declared global
trap_dispatcher_%+%1:                     ; Name e.g. `trap_dispatcher_0`
  pusha_sysv
  
  mov   rdi,%1                            ; Put the trap number in the first C argument
  mov   rsi,72[rsp]                       ; Error code from stack into the second C argument
  mov   rdx,80[rsp]                       ; Peek return address into third C argument
  call  handle_exception_wc               ; Call the with-code handler

  popa_sysv

  add   rsp,8                             ; Discard the error code

  iretq                                   ; And done...
%endmacro

%macro trap_dispatcher_no_code 1          ; General handler for traps that stack an error code
global trap_dispatcher_%+%1               ; Ensure declared global
trap_dispatcher_%+%1:                     ; Name e.g. `trap_dispatcher_1`
  pusha_sysv
  
  mov   rdi,%1                            ; Put the trap number in the first C argument (TODO changing registers!)
  mov   rsi,72[rsp]                       ; Peek return address into second C argument
  call  handle_exception_nc               ; Call the no-code handler

  popa_sysv

  iretq                                    ; And done...
%endmacro

trap_dispatcher_no_code      0             ; Division Error
trap_dispatcher_no_code      1             ; Debug
trap_dispatcher_no_code      2             ; NMI
trap_dispatcher_no_code      3             ; Breakpoint
trap_dispatcher_no_code      4             ; Overflow
trap_dispatcher_no_code      5             ; Bound Range Exceeded
trap_dispatcher_no_code      6             ; Invalid Opcode
trap_dispatcher_no_code      7             ; Device Not Available
trap_dispatcher_with_code    8             ; Double Fault
trap_dispatcher_no_code      9             ; Coprocessor Segment Overrun
trap_dispatcher_with_code    10            ; Invalid TSS
trap_dispatcher_with_code    11            ; Segment Not Present
trap_dispatcher_with_code    12            ; Stack-segment Fault
trap_dispatcher_with_code    13            ; GPF
trap_dispatcher_with_code    14            ; Page Fault
trap_dispatcher_no_code      15            ; <reserved>
trap_dispatcher_no_code      16            ; x87 Floating-point Exception
trap_dispatcher_with_code    17            ; Alignment Check
trap_dispatcher_no_code      18            ; Machine Check
trap_dispatcher_no_code      19            ; SIMD Floating-point Exception
trap_dispatcher_no_code      20            ; Virtualization Exception
trap_dispatcher_no_code      21            ; Control Protection Exception
trap_dispatcher_no_code      22            ; <reserved>
trap_dispatcher_no_code      23            ; <reserved> 
trap_dispatcher_no_code      24            ; <reserved>
trap_dispatcher_no_code      25            ; <reserved>
trap_dispatcher_no_code      26            ; <reserved>
trap_dispatcher_no_code      27            ; <reserved>
trap_dispatcher_no_code      28            ; Hypervisor Injection Exception
trap_dispatcher_no_code      29            ; VMM Communication Exception
trap_dispatcher_with_code    30            ; Security Exception
trap_dispatcher_no_code      31            ; <reserved>

; ISR dispatcher for PIC IRQs (all of which _should_ be spurious)
pic_irq_handler:
  push  rax                               ; Stash rax
  mov   rax,[spurious_irq_count]          ; Load the count
  inc   rax                               ; Increment it
  mov   [spurious_irq_count],rax          ; And store it back
  pop   rax                               ; Restore regs
  iretq                                   ; And done

; TODO I suspect we might need a separate handler here, specifically for PIC IRQ 15
; (vector 0x2f) because we should be sending EOI to the master PIC in that case...

timer_interrupt_handler:
  pusha_sysv

  call  handle_timer_interrupt            ; Just call directly to C handler

  popa_sysv
  iretq

; ISR dispatcher for Unknown (unhandled) IRQs
unknown_interrupt_handler:
  pusha_sysv

  call  handle_unknown_interrupt          ; Just call directly to C handler
  
  popa_sysv
  iretq

; Running count of spurious PIC IRQs
spurious_irq_count  dq  0
