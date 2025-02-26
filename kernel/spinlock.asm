; stage3 - Spinlocks
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
;

%ifdef asm_macho64
%define FUNC(name) _ %+ name
%else
%define FUNC(name) name
%endif

global FUNC(spinlock_init)
global FUNC(spinlock_lock), FUNC(spinlock_unlock)
global FUNC(spinlock_lock_irqsave), FUNC(spinlock_unlock_irqrestore)
global FUNC(spinlock_reentrant_init), FUNC(spinlock_reentrant_lock), FUNC(spinlock_reentrant_unlock)


; args:
;   rdi - *lock
; 
FUNC(spinlock_init):
    mov     qword [rdi], 0
    ret


; args:
;   rdi - *lock
; 
FUNC(spinlock_lock):
    xacquire lock bts qword [rdi], 0
    jc .wait
    ret

.wait:
    pause
    test qword [rdi], 1
    jz FUNC(spinlock_lock)
    jmp .wait


; args:
;   rdi - *lock
;
; ret:
;   rax - save flags
; 
FUNC(spinlock_lock_irqsave):
    pushfq
    cli
    pop     rax

.try:
    cli
    xacquire lock bts qword [rdi], 0
    jc .hold
    ret

.hold:
    push    rax
    popfq

.wait:
    pause
    test qword [rdi], 1
    jz .try
    jmp .wait


; args:
;   rdi - *lock
;
FUNC(spinlock_unlock):
    xrelease lock btr qword [rdi], 0; keeping the lock as it feels like requiring
                                    ; alignment of the lock will lead to subtle bugs..
    ret


; args:
;   rdi - *lock
;   rsi - restore flags
;
FUNC(spinlock_unlock_irqrestore):
    xrelease lock btr qword [rdi], 0; keeping the lock as it feels like requiring
                                    ; alignment of the lock will lead to subtle bugs..
    push    rsi 
    popfq
    ret


; args:
;   rdi - *lock
;   rsi - identity
;
FUNC(spinlock_reentrant_init):
    mov     qword [rdi], 0
    mov     qword [rdi+8], 0
    ret

; args:
;   rdi - *lock
;   rsi - identity
;
; modifies:
;   rax - 1 if lock taken, 0 otherwise
;
FUNC(spinlock_reentrant_lock):
    cmp qword [rdi+8], rsi          ; Is ident the owner of the lock?
    jz .nolock                      ; Just don't lock again if so...

.trylock:
    xacquire lock bts qword [rdi], 0; Try to set lock bit
    jc .wait                        ; If we couldn't set it, wait
    mov qword [rdi+8], rsi          ; else, store the ident

    mov rax,1                       ; and return 1
    ret

.wait:
    pause
    test qword [rdi], 1             ; Does it look unlocked now?
    jz .trylock                     ; yes - go try to lock it
    jmp .wait                       ; else no - spin

.nolock:
    xor rax,rax                     ; Return 0
    ret

; args:
;   rdi - *lock
;   rsi - identity
;
; modifies:
;   rax - 1 if lock released, 0 otherwise
;
FUNC(spinlock_reentrant_unlock):
    cmp qword [rdi+8], rsi          ; Is ident the owner of the lock?
    jnz .no_unlock                  ; Don't unlock if not...

    xrelease lock btr qword [rdi], 0; keeping the prefix as it feels like requiring
    mov qword [rdi+8],0             ; alignment of the lock could lead to subtle bugs..

    mov rax,1                       ; Unlocked - return 1
    ret

.no_unlock:
    xor rax,rax                     ; No unlock - return 0...
    ret
