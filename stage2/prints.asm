
; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Basic printing for early bootloader use...

bits 16

global real_print_sz

; Print sz string (16-bit real-mode edition, uses BIOS)
;
; Arguments:
;   SI - Should point to string
;
; Modifies:
;
;   SI
;
real_print_sz:
  push  ax
  push  bx

  mov   bh,0
  mov   ah,0x0e                           ; Set up for int 10 function 0x0e (TELETYPE OUTPUT)

.charloop:
  lodsb                                   ; get next char (increments si too)
  test  al,al                             ; Is is zero?
  je    .done                             ; We're done if so...
  int   0x10                              ; Otherwise, print it
  jmp   .charloop                         ; And continue testing...

.done:
  pop   bx
  pop   ax
  ret
