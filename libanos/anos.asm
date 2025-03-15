; system - anos syscalls interface for usermode
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

%macro anos_syscall 2
global %1_syscall, %1_int
%1_int:
    mov r9, %2              ; Call number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    int 0x69
    ret

%1_syscall:
    mov r9, %2
    mov r10, rcx
    syscall
    ret
%endmacro

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
anos_syscall anos_testcall, 0

; args:
;   rdi - message pointer
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_syscall anos_kprint, 1

; args:
;   rdi - character (low byte)
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_syscall anos_kputchar, 2

; args:
;   rdi - function pointer
;   rsi - user stack
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_syscall anos_create_thread, 3

; args:
;   rdi - AnosMemInfo pointer
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_syscall anos_get_mem_info, 4

; args:
;   rdi - nanos count
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;   
anos_syscall anos_task_sleep_current, 5

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
anos_syscall anos_create_process, 6

; args:
;   rdi - size
;   rsi - virtual_base
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_map_virtual, 7

; args:
;   rdi - destination
;   rsi - arg0
;   rdx - arg1
;
; mods:
;   rax - result
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_send_message, 8

; args:
;   rdi - source
;   rsi - arg0
;   rdx - arg1
;
; mods:
;   rax - message cookie
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_recv_message, 9

; args:
;   rdi - message cookie
;   rsi - arg0
;   rdx - arg1
;
; mods:
;   rax - message cookie
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_reply_message, 10

; args:
;   none
;
; mods:
;   rax - channel cookie, or 0 on failure
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_create_channel, 11

; args:
;   rdi - cookie
;
; mods:
;   rax - 0 on success, or negative on failure
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_destroy_channel, 12

; args:
;   rdi - cookie
;   rsi - name (char*)
;
; mods:
;   rax - 0 on success, or negative on failure
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_register_channel_name, 13

; args:
;   rdi - name (char*)
;
; mods:
;   rax - 0 on success, or negative on failure
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_remove_channel_name, 14

; args:
;   rdi - name (char*)
;
; mods:
;   rax - cookie on success, or 0 on failure
;   r11 - trashed
;   rcx - trashed
;
anos_syscall anos_find_named_channel, 15

