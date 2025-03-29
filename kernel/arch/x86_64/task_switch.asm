; stage3 - Assembler task switch handler
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;

bits 64
global task_do_switch
extern task_current_ptr, task_tss_ptr   ; TODO tss_ptr not used here now...
                                        ;      but needs setting up properly!

%define TASK_DATA   8                   ; Task struct offsets
%define TASK_RSP0   24
%define TASK_SSP    32
%define TASK_PML4   48

%define TSS_RSP0    4                   ; Offset of RSP0 in a TSS

%include "smp/state.inc"

; **Must** be called with scheduler locked!
;
; TODO this could be optimized (e.g. always fxsave/fxrstor, etc)
task_do_switch:
    mov     [temp_rsi],rsi

    mov     rsi,[gs:0]                      ; Load per-CPU data pointer
    add     rsi,CPU_TASK_CURRENT            ; Advance pointer to current task
    mov     rsi,[rsi]                       ; ... and load it

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

    mov     rsi,[gs:0]                      ; Load per-CPU data pointer
    add     rsi,CPU_TASK_CURRENT            ; Advance pointer to current task
    mov     rsi,[rsi]                       ; ... and load it

    mov     [rsi+TASK_SSP],rsp              ; Save stack pointer

    mov     rsi,[rsi+TASK_DATA]             ; Find task data pointer
    fxsave  [rsi]                           ; ... and just store fp state there.

.next:
    mov     rsi,[gs:0]                      ; Load per-CPU data pointer
    add     rsi,CPU_TASK_CURRENT            ; Advance pointer to current task
    mov     [rsi],rdi                       ; Load new task into variable

    ; NOTE: this next bit is why we **must** have two stacks even for kernel threads.
    ;       With just one stack, because we _always_ restore RSP0 to the top of the
    ;       kernel stack (which is what's stored in the task_ssp field), if we have
    ;       a single stack we'll overwrite / return to the wrong place...
    ;
    mov     rsp,[rdi+TASK_SSP]              ; Restore stack pointer (to kernel stack) 

    mov     rax,[rdi+TASK_RSP0]             ; Load saved RSP0 for the task

    mov     rsi,[gs:0]                      ; Load per-CPU data pointer
    add     rsi,CPU_TASK_TSS                ; Advance pointer to task TSS
    mov     rsi,[rsi]                       ; get TSS pointer into rsi
    mov     [rsi+TSS_RSP0],rax              ; ... and store kernel stack into TSS RSP0

    mov     rax, cr3                        ; Is CR3 different for the new task?
    mov     rcx, [rdi+TASK_PML4]
    cmp     rax,rcx
    je      .page_tables_done               ; If not, skip changing (so no TLB flush)...
    mov     cr3,rcx                         ; ... else, switch out cr3 with the new tables.

.page_tables_done:
    mov     rdi,[rdi+TASK_DATA]             ; Find new task data pointer
    fxrstor [rdi]                           ; ... and restore fp state from it

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