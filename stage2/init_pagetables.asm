; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Initial page-tables for jump to long mode
;

bits 32
global init_page_tables, PM4_START

; Initialise the initial page tables ready for long mode. Just identity maps
; the bottom 2MB for the switch - proper tables will be set up later...
;
; Call in protected mode.
;
; Modifies: Nothing
init_page_tables:
  push  eax                               ; Save registers
  push  ecx
  push  edi

  ; Zero out the page table memory
  mov   eax,0                             ; Value is zero
  mov   ecx,0x4000 / 4                    ; Count is number of dwords
  mov   edi,PM4_START                     ; Start at table start
  rep   stosd                             ; Zero

  ; Set up a single PM4 entry, pointing to the PDP
  mov   eax,PDP_START | PRESENT | WRITE
  mov   [PM4_START], eax

  ; Set up a single PDPT entry, pointing to the PD
  mov   eax,PD_START | PRESENT | WRITE
  mov   [PDP_START], eax

  ; Set up a single PD entry, pointing to PT
  mov   eax,PT_START | PRESENT | WRITE
  mov   [PD_START], eax

  ; Set up the page table to map the first 2MB
  mov   eax,PRESENT | WRITE               ; Start at addr 0, PRESENT | WRITE
  mov   edi,PT_START                      ; Filling the PT from the beginning

.pt_loop: 
  mov   [edi],eax                         ; Move current eax (address and flags) into the current PTE
  add   eax,0x1000                        ; Add 4K to eax, to point to the next table
  add   edi,8                             ; Move pointer to next PTE
  cmp   eax, 0x200000                     ; Have we done 2MB?
  jb    .pt_loop                          ; Keep looping if not...

  pop   edi                               ; Restore registers and done
  pop   ecx
  pop   eax
  ret


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Start of the page tables / top of stack
PM4_START   equ 0x9c000
PDP_START   equ 0x9d000
PD_START    equ 0x9e000
PT_START    equ 0x9f000

PRESENT     equ (1 << 0)
WRITE       equ (1 << 1)
