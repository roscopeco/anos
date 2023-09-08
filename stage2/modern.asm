; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Routines that ensure the machine meets our minimum needs...

bits 16
global guard_386, guard_long_mode, too_old

extern real_print_sz

; Check if the processor is a 386 or higher, and die (with a message) if not.
;
; Modifies: nothing
; Returns: sometimes
;
guard_386:
  push  ax                                ; Save registers
  push  cx        

  ; Is it a 286+?
  pushf                                   ; Push FLAGS
  pop   ax                                ; Pop FLAGS into AX
  mov   cx,ax                             ; Save original FLAGS for later restoration

  test  ax,0xC000                         ; Check bits 14 & 15
  jnz   too_old                           ; If set, CPU is too old... 

  ; Okay, but is it a 386+?
  or    ax,0x4000                         ; Set NT and IOPL flags
  push  ax                                ; push it
  popf                                    ; ... and pop into FLAGS
  pushf                                   ; Push FLAGS again
  pop   ax                                ; And get them back into AX
  test  ax,0x7000                         ; Are they still set?

  jz    too_old                           ; If not, CPU is too old...

  push  cx                                ; Otherwise, it's at least a 386. Restore original flags
  popf

  push  si
  mov   si,DOT                            ; Print a dot (wastefully)
  call  real_print_sz
  pop   si

  pop   cx                                ; Restore registers
  pop   ax

  ret                                     ; And we're done


; Check if the processor has long-mode support, and die (with a message) if not.
;
; Does 32-bit stuff in a real-mode world, so do guard_386 first.
;
; Modifies: nothing
; Returns: sometimes
;
guard_long_mode:
  push  ecx                               ; Save registers
  push  edx
  push  esi

  ; Is CPUID supported?
  pushfd                                  ; Push EFLAGS
  pop   ecx                               ; And pop into ECX
  mov   edx,ecx                           ; Copy into EDX for comparision

  xor   ecx,0x00200000                    ; Toggle ID bit
  push  ecx                               ; Push to stack
  popfd                                   ; ... back into EFLAGS
  pushfd                                  ; ... back onto stack
  pop   ecx                               ; ... and finally back into ECX

  cmp   ecx,edx                           ; Did the bit toggle?
  jz    too_old                           ; If not, CPU is too old (or just doesn't have CPUID, but whatever)...

  push  edx                               ; Restore original EFLAGS
  popfd

  mov   si,DOT                            ; Print a dot indicating CPUID is supported 
  call  real_print_sz

  ; Check extended function 0x80000001 is supported...
  mov   eax,0x80000000                    ; Check extended function
  cpuid                                   ; Do CPUID

  cmp   eax,0x80000001                    ; Is 0x80000001 supported?
  jb    too_old                           ; Too old (not enough CPUID support) if not

  mov   si,DOT                            ; Print a dot indicating 0x80000001 is supported
  call  real_print_sz

  mov   eax,0x80000001                    ; Use efunction 0x80000001
  cpuid                                   ; Do CPUID again

  test  edx,0x20000000                    ; Is bit 31 (LM) set?
  jz    too_old                           ; Too old if not...

  mov   si,GOOD                           ; Print CPU good message
  call  real_print_sz

  pop   esi                               ; Restore registers
  pop   edx
  pop   ecx 

  ret                                     ; And we have long mode, so carry on!


; Print the "Too old" message and halt forever.
;
too_old:
  mov   si,OLD                            ; Load "too old" message
  call  real_print_sz                     ; Print it
.die:
  hlt                                     ; Die...
  jmp   .die                              ; and stay dead


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
OLD   db  13, 10, "Sorry, your computer is not supported.", 0
DOT   db  ".", 0
GOOD  db  13, 10, 0
