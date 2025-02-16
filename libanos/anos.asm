; system - anos syscalls interface for usermode
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

global anos_testcall_int, anos_testcall_syscall
global anos_kputchar_syscall, anos_kputchar_int, anos_kprint_int, anos_kprint_syscall
global anos_create_thread_syscall, anos_create_thread_int
global anos_get_mem_info_syscall, anos_get_mem_info_int
global anos_task_sleep_current_syscall, anos_task_sleep_current_int
global anos_create_process_syscall, anos_create_process_int

; args:
;   rdi - arg0
;   rsi - arg1
;   rdx - arg2
;   r10 - arg3
;   r8  - arg4
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;
anos_testcall_int:
    xor r9, r9              ; Zero syscall number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    int 0x69
    ret


; args:
;   rdi - arg0
;   rsi - arg1
;   rdx - arg2
;   r10 - arg3
;   r8  - arg4
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_testcall_syscall:
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
anos_kprint_syscall:
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
anos_kprint_int:
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
anos_kputchar_syscall:
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
anos_kputchar_int:
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
;   rdi - nanos count
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
;   rdi - nanos count
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


; args:
;   rdi - stack address
;   rsi - stack size
;   rdx - bootstrap code start
;   r10 - bootstrap code len
;   r8  - entry point
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_create_process_syscall:
    mov r9, $6
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    syscall
    ret


; args:
;   rdi - stack address
;   rsi - stack size
;   rdx - bootstrap code start
;   r10 - bootstrap code len
;   r8  - entry point
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_create_process_int:
    mov r9, $6
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    int 0x69
    ret

