; system - anos syscalls interface for usermode
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

global testcall_int, testcall_syscall, kprint_int, kprint_syscall
global kputchar_syscall, kputchar_int


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
    xor r9, r9              ; Zero syscall number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
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
    xor r9, r9              ; Zero syscall number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
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
    mov r9, $1
    syscall
    ret


; args:
;   rdi - message pointer
;
; mods:
;   rax - result
;   
kprint_int:
    mov r9, $1
    int 0x69
    ret


; args:
;   rdi - character (low byte)
;
; mods:
;   rax - result
;   
kputchar_syscall:
    mov r9, $2
    syscall
    ret


; args:
;   rdi - character (low byte)
;
; mods:
;   rax - result
;   
kputchar_int:
    mov r9, $2
    int 0x69
    ret

