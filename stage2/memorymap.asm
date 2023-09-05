; stage2 - Boot sector stage 2 loader (memorymap.asm)
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Memory map builder based on BIOS INT15h AX=E820h

bits 16
global build_e820_memory_map

%define MAP_COUNT 0x8400
%define MAP_START 0x8402

; Get the memory map from BIOS function e820 and store it
; after the stage2 BSS at ES:0x8400.
;
; N.B. Doesn't actually _do_ anything with the map - it's
; just stored as-is in memory for later use once we've 
; switched modes and have more breathing space...
; 
; Result: 0x8400..0x8402 = DWORD Number of entries
;         0x8402..[????] = Entries (24-bytes each)
;
; Arguments: none
; Modifies: EAX (Returns number of entries loaded)
;           CF high indicates something might have gone wrong
;
; TODO this has got 6K of memory before it begins to 
; overwrite stage2 code, meaning there can be a max
; of 256 entries...
;
; _Seems_ like it should be enough...?
;
; TODO This isn't the only way to get memory (and it's not the
; most modern even for legacy boot - we should probably use ACPI
; or something at least - maybe if this one fails).
; 
; TODO I _think_ this is somewhat-subtly wrong. Specifically,
; according to docs, some BIOS won't return 0 in ECX on the last
; iteration, but will instead set the carry flag on the _next_
; call after the last iteration. 
;
; If this is right, it means there's a subtle difference between
; CF set on the _first_ call, vs CF set on subsequent calls, 
; which this won't handle right currently.
;
; It _will_ still return the count in AX, but stage2 can't 
; reasonably trust it if CF is set anyway, so it's kind of useless...
;
build_e820_memory_map:
  push  ebx
  push  ecx
  push  edx
  push  di

  xor   eax,eax                           ; Zero counter
  mov   [MAP_COUNT],eax

  xor   ebx,ebx                           ; EBX (continuation value) must start at zero
  mov   edx,0x0534D4150                   ; Signature ('SMAP') in EDX
  mov   di,MAP_START                      ; Start output at ES:0x8402

.loop:
  mov   eax,0xe820                        ; 0xE820 is the function code
  mov   ecx,0x18                          ; We're expecting entries of 24 bytes
  mov   [es:di + 0x14], dword 1	          ; Ensure this will _look_ like a valid ACPI 3.x entry, even if the BIOS only gives back 20 bytes...
  int   0x15                              ; Call it
  jc    .error                            ; CF indicates error (but still check EAX, doc is vague)

  mov   edx,0x0534D4150                   ; EAX expected to contain SMAP signature, restore in EDX as it might be trashed
  cmp   eax,edx                           ; Make sure it does!
  jne   .error                            ; We're done if not

  cmp   ecx,0x14                          ; Ensure BIOS seems to return sane entries
  jb    .error                            ; Entries must be >= 20 bytes

  cmp   ecx,0x18                          ; And <= 24 bytes
  ja    .error                            ; ...

  inc   dword [MAP_COUNT]                 ; Increment count
  add   di,0x18                           ; Point DI to next buffer : TODO do we need a bounds check?

  test  ebx,ebx                           ; Is EBX zero? 
  je    .complete                         ; This was the last entry if so
  
  jmp   .loop                             ; Otherwise, loop...

.complete:
  clc                                     ; Ensure carry flag is clear (no error)
  jmp   .done                             ; And done

.error:
  stc                                     ; Ensure carry flag is set (error)  

.done:
  mov   eax,[MAP_COUNT]                   ; Return count in EAX
  pop   di
  pop   edx
  pop   ecx
  pop   ebx
  ret
