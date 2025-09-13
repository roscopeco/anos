; stage3 - Common asm macros for ISR dispatch
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

%ifndef __ANOS_KERNEL_ISR_COMMON_INC
%define __ANOS_KERNEL_ISR_COMMON_INC

%define pusha_sysv_arg0 80                  ; Stack position of first non-saved register after pusha_sysv
%define pusha_sysv_arg1 88                  ; Stack position of second non-saved register after pusha_sysv

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro pusha_sysv_not_rax 0
    push  rcx                               ; Save all C-clobbered registers, except rax for returns
    push  rdx
    push  rsi
    push  rdi
    push  r8
    push  r9
    push  r10
    push  r11
    push  rbp
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro pusha_sysv 0
    push  rax                               ; Save all C-clobbered registers
    pusha_sysv_not_rax
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro popa_sysv_not_rax 0
    pop   rbp
    pop   r11                               ; Restore registers except rax for returns...
    pop   r10
    pop   r9
    pop   r8
    pop   rdi
    pop   rsi
    pop   rdx
    pop   rcx
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro popa_sysv 0
    popa_sysv_not_rax                       ; Restore registers...
    pop   rax
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro conditional_swapgs 1
%ifndef NO_USER_GS
    pushfq                                  ; Save flags

    cli                                     ; Still cli even for IRQ gates, this is used at the
                                            ; end of the handlers as well (when interrupts
                                            ; might've been re-enabled...)

    cmp qword [rsp+0x10+%1], 8              ; Are we coming from CS 8?
    jz %%.noswap                            ; We were already in kernel code if so, don't swap...
    swapgs                                  ; Otherwise, swap GS!
%%.noswap:
    popfq                                   ; Restore flags (including interrupt enable)
%endif
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro trap_conditional_swapgs_no_code 0
    conditional_swapgs 0x00                 ; No additional offset to stack
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro trap_conditional_swapgs_with_code 0
    conditional_swapgs 0x08                 ; Extra 8 bytes offset to stack, because error code...
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro irq_conditional_swapgs 0
    conditional_swapgs 0x00                 ; No additional offset to stack
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro trap_dispatcher_with_code 1          ; General handler for traps that stack an error code
global trap_dispatcher_%+%1                 ; Ensure declared global
trap_dispatcher_%+%1:                       ; Name e.g. `trap_dispatcher_0`
    cld
    trap_conditional_swapgs_with_code
    pusha_sysv                              ; Push all caller-saved registers

    mov   rdi,%1                            ; Put the trap number in the first C argument
    mov   rsi,80[rsp]                       ; Error code from stack into the second C argument
    mov   rdx,88[rsp]                       ; Peek return address into third C argument

    ; Set up stack frame (so we can do sane backtrace)...
    call  .stack_frame_setup                ; push $rip first
.stack_frame_setup:
    push  0                                 ; then push 0 for previous base pointer
    mov   rbp,rsp                           ; Set base pointer to point there
    call  handle_exception_wc               ; Call the with-code handler
    add   rsp,16                            ; Discard the stack frame

    popa_sysv                               ; Restore all caller-saved registers

    add   rsp,8                             ; Discard the error code

    trap_conditional_swapgs_no_code         ; We already discarded the code 👆
    iretq                                   ; And done...
%endmacro

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
%macro trap_dispatcher_no_code 1            ; General handler for traps that stack an error code
global trap_dispatcher_%+%1                 ; Ensure declared global
trap_dispatcher_%+%1:                       ; Name e.g. `trap_dispatcher_1`
    cld
    trap_conditional_swapgs_no_code
    pusha_sysv                              ; Push all caller-saved registers

    mov   rdi,%1                            ; Put the trap number in the first C argument (TODO changing registers!)
    mov   rsi,80[rsp]                       ; Peek return address into second C argument

    ; Set up stack frame (so we can do sane backtrace)...
    call  .stack_frame_setup                ; push $rip first
.stack_frame_setup:
    push  0                                 ; then push 0 for previous base pointer
    mov   rbp,rsp                           ; Set base pointer to point there
    call  handle_exception_nc               ; Call the no-code handler
    add   rsp,16                            ; Discard the stack frame

    popa_sysv                               ; Restore all caller-saved registers

    trap_conditional_swapgs_no_code
    iretq                                   ; And done...
%endmacro

%endif