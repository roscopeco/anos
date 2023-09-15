; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Initial (super basic) initialization of C runtime
;
; This only does the bare minimum - there's no global constructors
; or destructors, for example.
;
; .data is copied, and .bss is zeroed. That's it.
;
; This **must not** be called until we're completely done with stage1
; data (i.e. have finished loading files) since it will overwrite 
; that memory for .data and .bss!

bits 64
global init_c_land

extern _code_end                          ; Linker defined symbols
extern _data_start, _data_end             ; ...
extern _bss_start, _bss_end               ; ...


; Initialize C-land: Zero BSS...
;
; Modifies: nothing
;
init_c_land:
  push  rax                               ; Save registers
  push  rbx
  push  rcx
  push  rdx

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

.done:
  pop   rdx                               ; Restore registers
  pop   rcx
  pop   rbx
  pop   rax
  ret




