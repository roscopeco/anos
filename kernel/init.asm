; stage3 - Kernel initializer
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Initial (super basic) initialization of C runtime and calling out
; to kernel startup.
;
; This only does the bare minimum - there's no global constructors
; or destructors, for example.
;
; .bss is zeroed, and the C entry poit is called. That's it.
;
; What it expects:
;
;   * Entry point at 0x00120000 in 64-bit long mode (not compatibility)
;   * Stack already set up somewhere (we'll move it in a bit)
;   * Interrupts disabled
;   * VGA 80x26 16-color text mode
;   * RDI contains address of memory map from stage2
; 
; What it does:
;
;   * Zeroes BSS
;   * Calls out to the C entry point
;

bits 64
global _start

extern start_kernel
extern _bss_start, _bss_end               ; Linker defined symbols

section .text.init                        ; Linker needs to make sure this goes in first...

; Initialize C-land: Zero BSS, sort out memory pointer argument and call main
;
_start:
  mov   rcx,_bss_end                      ; Get end of .bss section (VMA)
  mov   rax,_bss_start                    ; Get start of .bss section (VMA)
  sub   rcx,rax                           ; Compute length of .bss (bytes) in RCX
  shr   rcx,0x2                           ; Divide by 4 (we're zeroing dwords)

  test  rcx,rcx                           ; Do we have zero-size .bss?
  jz    .done                             ; We're done if so...

  mov   rbx,_bss_start                    ; bss start (VMA) into rbx
.zero_bss_loop:
  mov   dword [rbx],0x0                   ; Clear one dword
  add   rbx,0x4                           ; Increment write pointer
  dec   rcx                               ; Decrement loop counter
  jnz   .zero_bss_loop                    ; Loop until CX is zero

.done
  jmp   start_kernel                      ; Let's do some C...





