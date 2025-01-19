; system - anos syscalls interface for usermode
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

global testcall_int, testcall_syscall
global kputchar_syscall, kputchar_int, kprint_int, kprint_syscall
global anos_create_thread_syscall, anos_create_thread_int
global anos_get_mem_info_syscall, anos_get_mem_info_int
global anos_task_sleep_current_syscall, anos_task_sleep_current_int


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
;   r11 - trashed
;   rcx - trashed
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
;   r11 - trashed
;   rcx - trashed
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
;   r11 - trashed
;   rcx - trashed
;   
kputchar_int:
    mov r9, $2
    int 0x69
    ret


; args:
;   rdi - function pointer
;   rsi - user stack
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_create_thread_syscall:
    mov r9, $3
    syscall
    ret


; args:
;   rdi - function pointer
;   rsi - user stack
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_create_thread_int:
    mov r9, $3
    int 0x69
    ret


; args:
;   rdi - AnosMemInfo pointer
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_get_mem_info_syscall:
    mov r9, $4
    syscall
    ret


; args:
;   rdi - AnosMemInfo pointer
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_get_mem_info_int:
    mov r9, $4
    int 0x69
    ret


; args:
;   rdi - tick count
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_task_sleep_current_syscall:
    mov r9, $5
    syscall
    ret


; args:
;   rdi - tick count
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_task_sleep_current_int:
    mov r9, $5
    int 0x69
    ret

