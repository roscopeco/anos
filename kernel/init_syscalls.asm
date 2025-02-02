; stage3 - SYSCALL / SYSRET
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;F
; Setup and entrypoint / dispatch for SYSCALLs
;

bits 64

global syscall_init
extern handle_syscall_69

%define MSR_EFER    0xc0000080
%define MSR_STAR    0xc0000081
%define MSR_LSTAR   0xc0000082
%define MSR_SFMASK  0xc0000084

%define EFLAGS_IF   1 << 9
%define EFLAGS_IOPL 3 << 12
%define EFLAGS_NT   1 << 14
%define EFLAGS_RF   1 << 16
%define EFLAGS_VM   1 << 17
%define EFLAGS_AC   1 << 18
%define EFLAGS_VIF  1 << 19
%define EFLAGS_VIP  1 << 20
%define EFLAGS_ID   1 << 21

%include "smp/state.inc"

%define TASK_RSP0   24

%macro write_msr 2
   mov rcx, %1
   mov rax, %2
   mov rdx, rax
   shr rdx, 32
   wrmsr
%endmacro

; Initialize the MSRs to enable SYSCALL
syscall_init:
    ; Save regs
    push    rdx
    push    rcx
    push    rax

    ; Set up SYSCALL entry point
    write_msr MSR_LSTAR, syscall_enter

    ; Set up the SYSCALL code segments - User @ 0x10 (+8), kernel @ 0x08
    write_msr MSR_STAR, 0x0010000800000000

    ; Set up the SFMASK to mask out:
    ;
    ; * IF (bit 9): Interrupts
    ; * IOPL (bits 12-13): I/O privilege
    ; * NT (bit 14): Task flag
    ; * RF (bit 16): Resume flag
    ; * VM (bit 17): Virtual 8086
    ; * AC (bit 18): Alignment check
    ; * VIF (bit 19): Virtual IF
    ; * VIP (bit 20): Virtual IP
    ; * ID (bit 21): CPUID flag
    ;
    write_msr 0xC0000084, EFLAGS_IF | EFLAGS_IOPL | EFLAGS_NT | EFLAGS_RF | EFLAGS_VM | EFLAGS_AC | EFLAGS_VIF | EFLAGS_VIP | EFLAGS_ID

    ; Enable the syscall instruction (bit 0 in EFER MSR)
    mov rcx, 0xC0000080    ; EFER MSR
    rdmsr
    or eax, 1              ; Set SCE bit
    wrmsr

    ; Restore regs
    pop     rax
    pop     rcx
    pop     rdx
    ret


; Entry point for SYSCALLs
syscall_enter:
%ifndef NO_USER_GS
    cli
    nop
    swapgs

    mov     r11,[gs:0]                      ; Load per-CPU data pointer
    mov     r11,[r11]                       ; Dereference it
    mov     [r11+CPU_RSP_STASH],rsp         ; Stash the user stack pointer
    mov     r11,[r11+CPU_TASK_CURRENT]      ; ... and load task pointer
    mov     rsp,[r11+TASK_RSP0]             ; ... so we can switch to kernel stack
%endif

    push rbp
    mov rbp, rsp
    push rcx        ; Return addr
    push rbx
    push r11        ; syscall clobbers this...
    push r12
    push r13
    push r14
    push r15
    
    mov rcx, r10    ; 4th arg from r10
    sti
    call handle_syscall_69
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop rbx
    pop rcx
    pop rbp
%ifndef NO_USER_GS
    cli
    mov     r11,[gs:0]                      ; Load per-CPU data pointer
    mov     r11,[r11]                       ; Dereference it
    mov     rsp,[r11+CPU_RSP_STASH]         ; ... and restore the user stack pointer

    swapgs
%endif
    o64 sysret