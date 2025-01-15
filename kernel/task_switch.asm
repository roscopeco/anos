; stage3 - Assembler task switch handler
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;

bits 64
global task_do_switch
extern task_current_ptr, task_tss_ptr

%define TASK_TID    16
%define TASK_RSP0   24
%define TASK_SSP    32
%define TASK_PML4   48

%define TSS_RSP0    4

; **Must** be called with interrupts disabled!
task_do_switch:
    mov     [temp_rsi],rsi
    mov     rsi,[task_current_ptr]          ; Get current task struct
    test    rsi,rsi
    jz      .next                           ; If it's NULL, don't save anything...
    mov     rsi,[temp_rsi]

    ; TODO don't need to save all these, caller saved some of them...
    push    rax                             ; Push all GP registers
    push    rbx
    push    rcx
    push    rdx
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    push    rsi                             ; Order matters! task_user_entrypoint relies
    push    rdi                             ; on rsi/rdi order for arguments here!

    mov     rsi,[task_current_ptr]          ; Get current task struct
    mov     [rsi+TASK_SSP],rsp              ; Save stack pointer

.next:
    mov     [task_current_ptr],rdi          ; Load new task into variable

    mov     rsp,[rdi+TASK_SSP]              ; Restore stack pointer (to kernel stack) 

    mov     rax,[rdi+TASK_RSP0]
    mov     rsi,[task_tss_ptr]              ; get TSS pointer into rsi
    mov     [rsi+TSS_RSP0],rax              ; ... and store kernel stack into RSP0

    mov     rax, cr3                        ; Is CR3 different for the new task?
    mov     rcx, [rdi+TASK_PML4]
    cmp     rax,rcx
    je      .page_tables_done               ; If not, skip changing (so no TLB flush)...
    mov     cr3,rcx                         ; ... else, switch out cr3 with the new tables.

.page_tables_done:
    pop     rdi                             ; Pop all GP registers
    pop     rsi
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ret

    section .bss
temp_rsi    resq  1