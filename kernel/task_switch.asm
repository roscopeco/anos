; stage3 - Assembler task switch handler
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;

bits 64
global task_do_switch
extern task_current_ptr

%define TASK_TID    0
%define TASK_SP     8

task_do_switch:
    cli                                     ; Disable interrupts

    mov     [temp_rsi],rsi
    mov     rsi,[task_current_ptr]          ; Get current task struct
    test    rsi,rsi
    jz      .next
    mov     rsi,[temp_rsi]

    push    rax                             ; Push all GP registers
    push    rbx
    push    rcx
    push    rdx
    push    rbp
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    pushfq                                  ; Push flags

    mov     rsi,[task_current_ptr]          ; Get current task struct
    mov     [rsi+TASK_SP],rsp               ; Save stack pointer

.next:
    mov     [task_current_ptr],rdi          ; Load new task into variable
    mov     rsp,[rdi+TASK_SP]               ; Restore stack pointer

    popfq                                   ; Pop flags
    pop     r15                             ; Pop all GP registers
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    sti                                     ; Re-enable interrupts and return
    ret

    section .bss
temp_rsi    resq  1