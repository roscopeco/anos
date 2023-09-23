; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Second-stage loader. Stage 1 loads this at 0x9c00 and goes from there...
;
; What it expects:
;
;   * Entry point at 0x0000:9c00 in real mode
;   * Real-mode segments set up (including a small, usable stack somewhere)
;   * Interrupts disabled
;   * FAT boot sector (i.e. stage 1, but with BPB / EBPB also) at 0000:7c00
; 
; What it does:
;
;   * Entry in real mode (from stage1)
;     * Prints a message as proof of life (with BIOS routines)
;     * Checks the CPU supports long mode (and dies with a message if not)
;     * Grabs a memory map from BIOS E820h and leaves it in low RAM
;     * Sets up the GDT with both 32- and 64-bit code / data segments
;   * Enables Unreal mode for a bit
;     * Loads stage3 at 0x00120000 using BIOS routines (still has to copy because using BIOS floppy)
;   * Enables 32-bit protected mode for a short time
;     * Sets up data and stack segments for protected mode
;     * Prints another message (direct to VGA mem, because, why not?)
;     * Checks if A20 is enabled, and tries to enable it (keyboard controller method only right now)
;     * Builds a basic page table to identity-map the first 2MB, so we can...
;   * Enable 64-bit long mode
;     * Does a bit more printing (I like printing, okay?)
;     * Jumps to stage 3
;
; What it doesn't:
;
;   * Disable IRQ or NMI - works for now on the emulators, but will need it later
;   * Set up IDT - Didn't want to bother in 32-bit mode, I'll take the triple-faults...
;
; Notes:
;
;   * As implemented, this **must** be loaded into segment 0x0!
;     * If I want to change that, I'll need to either change the segment bases for 32/64 bits,
;       or have separate sections that are linked at different base addresses in an LD script...
;

global _start                             ; Shut up NASM, I (imagine I) know what I'm doing... 🙄🤣

extern real_print_sz                      ; Defined in prints.asm
extern build_e820_memory_map              ; Defined in memorymap.asm
extern check_a20, enable_a20              ; Defined in a20.asm
extern guard_386, guard_long_mode, too_old; Defined in modern.asm
extern load_stage3                        ; Defined in fat.asm
extern init_page_tables                   ; Defined in init_pagetables.asm

extern PM4_START                          ; Variable, Defined in init_pagetables.asm


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 16-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 16

_start:
  xor   ax,ax                             ; Zero ax
  mov   ds,ax                             ; Set up data segment...

  mov   si,MSG                            ; Load message
  call  real_print_sz                     ; And print it...
  call  guard_386                         ; Ensure we have at least a 386 processor before doing 32-bit stuff

  call  guard_long_mode                   ; Now ensure we have a processor with long mode, might as well die early if not...

  mov   ax,0xec00                         ; Let the BIOS know we're planning to go to long mode  
  mov   bl,0x02                           ; (Target Operating Mode Callback)  - Neither Bochs nor qemu
  int   0x15                              ; appear to support this, so give back CF=1 and AH=0x86...

.unreal:
  ; Jump to unreal mode
  push  ds                                ; Save DS and ES (both should be zero anyway)
  push  es

  lgdt  [GDT_DESC]                        ; Load GDT reg with the descriptor

  mov   eax,cr0                           ; Get control register 0 into eax
  or    al,1                              ; Set PR bit (enable protected mode)
  mov   cr0,eax                           ; And put back into cr0
  
.enter_unreal:
  mov   bx,0x10                           ; Loading segment 2 (32-bit data)...
  mov   ds,bx                             ; ... into DS
  mov   es,bx
  
  and   al,0xFE                           ; Back to real mode (unreal mode)
  mov   cr0,eax

  jmp   0x00:.in_unreal                   ; Far jump back to unreal mode


; Entry point into unreal mode (for loading stage 3)
.in_unreal:
  pop   es
  pop   ds                                ; (... base from 0, where 32-bit segment starts)

  call  load_stage3                       ; Let's do the stage3 load now we're in unreal mode...
                                          ; TODO This isn't safe! Real BIOS can kick us back to real mode...

  call  build_e820_memory_map             ; Looks like we're good, so let's grab the mem map before leaving real mode.
  jc    .too_bad                          ; Just print message and die on fail (see also comments in memorymap.asm!)

  mov ah, 0x00                            ; One last thing before we leave (un)real mode forever...
  mov al, 0x03                            ; Set video to text mode 80x25 16 colours,
  int 0x10                                ; which will clear the screen...

.protect:
  ; Jump to protected mode for reals this time
  mov   eax,cr0                           ; Get control register 0 into eax
  or    al,1                              ; Set PR bit (enable protected mode)
  mov   cr0,eax                           ; And put back into cr0

  jmp   0x08:main32                       ; Far jump to main32, use GDT selector at offset 0x08.

.too_bad:
  call  too_old

.die:
  hlt                                     ; Die...
  jmp   .die                              ; and stay dead


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 32-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 32

; Entry point into protected mode proper, for setting up long mode
main32:
  mov   ax,0x10                           ; Set ax to 0x10 - offset of segment 2, the 32-bit data segment...
  mov   ds,ax                             ; and set DS to that..
  mov   es,ax                             ; and ES too...
  mov   ss,ax                             ; as well as SS.
  mov   esp,PM4_START                     ; Stack below page tables (16k below top of free low RAM)

  call  enable_a20                        ; Enable A20 gate
  call  init_page_tables                  ; Init basic (identity) page tables for long-mode switch
  jmp   go_long


; Go long mode. This jumps to main64 at the end, so it's noreturn.
; Just jump to it rather than calling it :) 
go_long:  
  mov   eax,0b10100000                    ; Set PAE and PGE bits
  mov   cr4,eax

  mov   eax,PM4_START                     ; Load PML3 into cr3
  mov   cr3,eax

  mov   ecx,0xC0000080                    ; Read EFER MSR
  rdmsr

  or    eax,0x100                         ; Set the long-mode enable bit
  wrmsr                                   ; and write back to EFER MSR

  mov   eax,cr0                           ; Activate paging to go full long mode
  or    eax,0x80000000                    ; (we're already in protected mode)
  mov   cr0,eax

  jmp   0x18:main64                       ; Go go go


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 64-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 64
main64:
  mov   ax,0x20                           ; Set ax to 0x20 - offset of segment 4, the 64-bit data segment...
  mov   ds,ax                             ; and set DS to that..
  mov   es,ax                             ; and ES too...
  mov   fs,ax                             ; and FS too...
  mov   gs,ax                             ; and GS too...
  mov   ss,ax                             ; as well as SS.

  mov   rsp,0xFFFFFFFF80110000            ; Reset stack, and move it to kernel address space at bottom of BSS

  mov   byte [0xb8002],'L'                ; Print "L"
  mov   byte [0xb8003],0x2a               ; In color

  mov   rdi,0xFFFFFFFF80008400            ; Memory map was loaded at 0x8400, pass kernel-space mapping into stage3
  mov   rbx,STAGE_3_HI_ADDR                  
  jmp   rbx                               ; Finally, jump to stage3 at high address, which we loaded earlier 🥳


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%defstr version_str VERSTR
MSG   db  "ANLOAD #", version_str, 0

GDT:
  ; segment 0 - null
  dq 0

  ; segment 1 - 32-bit code
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 2 - 32-bit data
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 1 - 64-bit code
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b10101111           ; Flags + Limit: 1 = 4k granularity, 0 = 16-bit, 1 = Long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 2 - 64-bit data
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b10101111           ; Flags + Limit: 1 = 4k granularity, 0 = 16-bit, 1 = Llong mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

GDT_DESC:
  ; GDT Descriptor
  dw  GDT_DESC-GDT-1      ; Size (computed from here - start)
  dd  GDT                 ; Address (GDT, above)

