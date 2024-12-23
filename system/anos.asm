; system - anos syscalls
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

global kprint_int, kprint_syscall

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


