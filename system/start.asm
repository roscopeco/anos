;
; system - Pre-main startup code
; anos - An Operating System
;
; Copyright (c) 2024 Ross Bamford
; This is the entry-point for the user-mode supervisor...
;
 
bits 64
global _start

extern main
extern _bss_start, _bss_end               ; Linker defined symbols

section .text.init                        ; Linker needs to make sure this goes in first...

; Initialize C-land: Zero BSS, sort out arguments and call main
;
_start:
  mov   rcx,_bss_end                        ; Get end of .bss section (VMA)
  mov   rax,_bss_start                      ; Get start of .bss section (VMA)
  sub   rcx,rax                             ; Compute length of .bss (bytes) in RCX
  shr   rcx,0x2                             ; Divide by 4 (we're zeroing dwords)

  test  rcx,rcx                             ; Do we have zero-size .bss?
  jz    .done                               ; We're done if so...

  mov   rbx,_bss_start                      ; bss start (VMA) into rbx
.zero_bss_loop:
  mov   dword [rbx],0x0                     ; Clear one dword
  add   rbx,0x4                             ; Increment write pointer
  dec   rcx                                 ; Decrement loop counter
  jnz   .zero_bss_loop                      ; Loop until CX is zero

.done:
  mov   rdi, 0                              ; argc = 0
  mov   rsi, EMPTY_ARGS                     ; argv = pointer to null array
  jmp   main                                ; Let's do some C...

EMPTY_ARGS:
    dq  0