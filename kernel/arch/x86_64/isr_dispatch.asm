; stage3 - Kernel ISR dispatchers (to C handlers)
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; ISR dispatchers for calling C code in ISRs
;

bits 64

%include "kernel/arch/x86_64/isr_common.asm"

global pic_irq_handler, unknown_interrupt_handler, spurious_irq_count
global bsp_timer_interrupt_handler, ap_timer_interrupt_handler
global syscall_69_handler
global page_fault_dispatcher
global double_fault_dispatcher

extern handle_exception_nc, handle_exception_wc, handle_unknown_interrupt
extern handle_bsp_timer_interrupt, handle_ap_timer_interrupt, 
extern handle_syscall_69
extern page_fault_handler
extern double_fault_handler

trap_dispatcher_no_code      0              ; Division Error
trap_dispatcher_no_code      1              ; Debug
trap_dispatcher_no_code      2              ; NMI
trap_dispatcher_no_code      3              ; Breakpoint
trap_dispatcher_no_code      4              ; Overflow
trap_dispatcher_no_code      5              ; Bound Range Exceeded
trap_dispatcher_no_code      6              ; Invalid Opcode
trap_dispatcher_no_code      7              ; Device Not Available
trap_dispatcher_with_code    8              ; Double Fault
trap_dispatcher_no_code      9              ; Coprocessor Segment Overrun
trap_dispatcher_with_code    10             ; Invalid TSS
trap_dispatcher_with_code    11             ; Segment Not Present
trap_dispatcher_with_code    12             ; Stack-segment Fault
trap_dispatcher_with_code    13             ; GPF
trap_dispatcher_with_code    14             ; Page Fault
trap_dispatcher_no_code      15             ; <reserved>
trap_dispatcher_no_code      16             ; x87 Floating-point Exception
trap_dispatcher_with_code    17             ; Alignment Check
trap_dispatcher_no_code      18             ; Machine Check
trap_dispatcher_no_code      19             ; SIMD Floating-point Exception
trap_dispatcher_no_code      20             ; Virtualization Exception
trap_dispatcher_no_code      21             ; Control Protection Exception
trap_dispatcher_no_code      22             ; <reserved>
trap_dispatcher_no_code      23             ; <reserved>
trap_dispatcher_no_code      24             ; <reserved>
trap_dispatcher_no_code      25             ; <reserved>
trap_dispatcher_no_code      26             ; <reserved>
trap_dispatcher_no_code      27             ; <reserved>
trap_dispatcher_no_code      28             ; Hypervisor Injection Exception
trap_dispatcher_no_code      29             ; VMM Communication Exception
trap_dispatcher_with_code    30             ; Security Exception
trap_dispatcher_no_code      31             ; <reserved>

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

bsp_timer_interrupt_handler:
    irq_conditional_swapgs
    pusha_sysv

    ; TODO stack alignment?

    call  handle_bsp_timer_interrupt        ; Just call directly to C handler

    popa_sysv
    irq_conditional_swapgs
    iretq

ap_timer_interrupt_handler:
    irq_conditional_swapgs
    pusha_sysv

    ; TODO stack alignment?

    call  handle_ap_timer_interrupt         ; Just call directly to C handler

    popa_sysv
    irq_conditional_swapgs
    iretq

syscall_69_handler:
    irq_conditional_swapgs                  ; Syscalls are set up to mask interrupts...
    pusha_sysv_not_rax

    ; Stack should already be 16-byte aligned here...

    mov   rcx,r10
    call  handle_syscall_69                 ; Just call directly to C handler

    popa_sysv_not_rax
    irq_conditional_swapgs
    iretq

; ISR dispatcher for Unknown (unhandled) IRQs
unknown_interrupt_handler:
    irq_conditional_swapgs
    pusha_sysv

    ; TODO stack alignment?

    call  handle_unknown_interrupt          ; Just call directly to C handler

    popa_sysv
    irq_conditional_swapgs
    iretq

; Running count of spurious PIC IRQs
spurious_irq_count  dq  0

; Page fault handler (only once task subsystem is up)
page_fault_dispatcher:
    trap_conditional_swapgs_with_code
    pusha_sysv                              ; Push all caller-saved registers

    mov   rdi,pusha_sysv_arg0[rsp]          ; Error code from stack into the first C argument
    mov   rsi,cr2                           ; Fault address into the second C argument
    mov   rdx,pusha_sysv_arg1[rsp]                           ; Peek return address into third C argument

    ; Set up stack frame (so we can do sane backtrace)...
    call  .stack_frame_setup                ; push $rip first
.stack_frame_setup:
    push  0                                 ; then push 0 for previous base pointer
    mov   rbp,rsp                           ; Set base pointer to point there
    call  page_fault_handler                ; Call the with-code handler
    add   rsp,16                            ; Discard the stack frame

    popa_sysv                               ; Restore all caller-saved registers

    add   rsp,8                             ; Discard the error code

    trap_conditional_swapgs_no_code         ; We already discarded the code 👆
    iretq                                   ; And done...

; Double fault handler
double_fault_dispatcher:
    irq_conditional_swapgs
    pusha_sysv                              ; Push all caller-saved registers

    mov   rdi,80[rsp]                       ; Peek return address into first C argument

    ; Set up stack frame (so we can do sane backtrace)...
    call  .stack_frame_setup                ; push $rip first
.stack_frame_setup:
    push  0                                 ; then push 0 for previous base pointer
    mov   rbp,rsp                           ; Set base pointer to point there
    call  double_fault_handler              ; Call the handler

.die:
    hlt                                     ; handler should never return...
    jmp   .die                              ; ... but just in case...

