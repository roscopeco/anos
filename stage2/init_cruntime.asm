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

bits 64
global init_c_land

extern _code_end                          ; Linker defined symbols
extern _data_start, _data_end             ; ...
extern _bss_start, _bss_end               ; ...


; Initialize C-land: copy data and zero BSS...
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
  push  rcx                               ; Stash this away for later...

  mov   rcx,_data_end                     ; Get end of .data section (VMA)
  mov   rax,_data_start                   ; Get start of .data section (VMA)
  sub   rcx,rax                           ; Compute length of .data (bytes) in RCX
  shr   rcx,0x2                           ; Divide by 4 (we're copying dwords)

  test  rcx,rcx                           ; Do we have zero-size .data?
  jz    .zero_bss                         ; Skip to zero BSS if so...

  mov   rbx,_code_end                     ; End of .text (i.e. start of loaded .data) into rbx
.copy_data:
  mov   edx, dword [rbx]                  ; Read from end of .text
  mov   dword [rax],edx                   ; Write to VMA of .data
  add   rax,0x4                           ; increment write pointer by one dword
  add   rbx,0x4                           ; increment read pointer by one dword
  dec   rcx                               ; Decrement loop counter
  jnz   .copy_data                        ; Loop until CX is zero

.zero_bss:
  pop   rcx                               ; pop bss size back into RCX
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




