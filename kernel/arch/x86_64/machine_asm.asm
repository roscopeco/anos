; stage3 - Machine functions implemented in assembly
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

global get_new_thread_entrypoint, get_new_thread_userstack
global get_cpu_flags

get_new_thread_entrypoint:
    mov rax, r15
    ret

get_new_thread_userstack:
    mov rax, r14
    ret

get_cpu_flags:
    pushfq
    pop rax
    ret