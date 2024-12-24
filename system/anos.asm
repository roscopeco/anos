; system - anos syscalls interface for usermode
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

global testcall_int, testcall_syscall, kprint_int, kprint_syscall


; args:
;   rdi - arg0
;   rsi - arg1
;   rdx - arg3
;   r10 - arg4
;   r8  - arg5
;   r9  - arg6
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;
testcall_int:
    xor rax,rax
    mov rdi, $1
    mov rsi, $2
    mov rdx, $3
    mov r10, $4
    mov r8, $5
    int 0x69
    ret


; args:
;   rdi - arg0
;   rsi - arg1
;   rdx - arg3
;   r10 - arg4
;   r8  - arg5
;   r9  - arg6
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
testcall_syscall:
    xor rax,rax
    mov rdi, $1
    mov rsi, $2
    mov rdx, $3
    mov r10, $4
    mov r8, $5
    syscall
    ret


; args:
;   rdi - message pointer
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
kprint_syscall:
    mov rax,$1
    syscall
    ret


; args:
;   rdi - message pointer
;
; mods:
;   rax - result
;   
kprint_int:
    mov rax, $1
    int 0x69
    ret



