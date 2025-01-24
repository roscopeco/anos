; stage3 - C-callable CPUID
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

global init_cpuid, cpuid
global __cpuid_vendor

init_cpuid:
    push rbx                        ; Save regs

    xor eax,eax                     ; Clear eax
    cpuid
    mov byte [cpuid_max],byte al    ; Store highest CPUID function
    mov [__cpuid_vendor],ebx        ; Store vendor string...
    mov [__cpuid_vendor+4],edx
    mov [__cpuid_vendor+8],ecx
    mov byte [__cpuid_vendor+12],$0

    pop rbx
    ret

cpuid:
    mov rax,rdi                     ; Func num in rax
    cmp al,byte [cpuid_max]         ; Does CPU support it?
    ja  .bad_func                   ; ... bad func if not...

    push rbx                        ; Save rbx

                                    ; rax ptr is in rsi
    mov r9,rdx                      ; stash pointer for rbx in r9
    mov r10,rcx                     ; stash pointer for rcx in r10
    mov r11,r8                      ; stash pointer for rdx in r11

    cpuid

    mov [rsi],eax                   ; rax into rax pointer
    mov [r9],ebx                    ; rbx into rbx pointer
    mov [r10],ecx                   ; rcx into rcx pointer
    mov [r11],edx                   ; rdx into rdx pointer    

    mov rax,$1                      ; return success

    pop rbx
    ret

.bad_func:
    xor rax,rax                     ; return fail
    ret

section .bss

__cpuid_vendor  db  13
cpuid_max       db  1

section .text

