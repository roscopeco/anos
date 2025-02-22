; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Initial page-tables for jump to long mode
;
; Kernel **code** space is the top 2GB (or, the negative 2GB if you prefer),
; so the base address for that is 0xFFFFFFFF80000000. 
;
; However, the top 512GB is allocated as kernel-reserved space, so kernel
; space actually begins at 0xFFFFFF8000000000, i.e. the whole last PML4
; entry.
;
; I load the Kernel in unreal mode to physical 0x120000 (just above 1MiB), 
; leaving 128KB at the bottom of that MiB for BSS etc.
;
; For an easy life, in these initial page tables I'm just going to identity map
; the first 2MB (so the running code doesn't blow up when I switch paging on)
; and also map the first 2MB of kernel space to the bottom 2MB too (so when
; I jump to stage 3 I can just go to 0xFFFFFFFF80120000 which will be mapped 
; to the same buffer I loaded the kernel to, at 0x120000 physical.
;
; For this reason, the kernel is linked to run at 0xFFFFFFFF80120000, with
; the BSS segment linked at 0xFFFFFFFF80100000. The bottom 1MB of kernel space
; is mapped to the first megabyte of RAM, which might (or might not) be useful
; later.
;
; This is how the address 0xFFFFFFFF80000000 breaks down in the page tables:
;
; 1111111111111111 111111111 111111110 000000000 000000000 000000000000
;  [16-bit unused]    PM4       PDPT       PD       PT       [Offset]
;
; I expect that having the kernel sat at ~1MiB physical is gonna come back
; to bite me at some point because DMA or something, but it's fine for now...
;
; This also means the page tables themselves can still be accessed via
; 0x9c000, and also now via 0xFFFFFFFF8009c000. Probably not much use, 
; since I'll likely build a whole new set later when it's time for the 
; kernel to set them up properly, but might be handly...
;

bits 32
global init_page_tables, PM4_START

; Initialise the initial page tables ready for long mode.
;
; Call in protected mode.
;
; Modifies: Nothing
;
init_page_tables:
  push  eax                               ; Save registers
  push  ecx
  push  edi

  ; Zero out the page table memory
  mov   eax,0                             ; Value is zero
  mov   ecx,0x4000                        ; Count is number of dwords
  mov   edi,PM4_START                     ; Start at table start
  rep   stosd                             ; Zero

  ; Set up a single PM4 entry, pointing to the PDP
  mov   eax,PDP_START | PRESENT | WRITE
  mov   [PM4_START], eax                  ; Identity map (first PM4 entry)
  mov   [PM4_START+0xFF8], eax            ; Also kernel space (last PM4 entry)

  ; Set up a single PDPT entry, pointing to the PD (this will introduce a copy at 512GiB, but whatever...)
  mov   eax,PD_START | PRESENT | WRITE
  mov   [PDP_START], eax                  ; Identity map (first PDPT entry)
  mov   [PDP_START+0xFF0], eax            ; Also kernel space (second-to-last PDPT entry)

  ; Set up a single PD entry, pointing to PT
  mov   eax,PT_START | PRESENT | WRITE
  mov   [PD_START], eax                   ; From PD onward, both low and high addresses use the first entry 🙂

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
; Equates
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Start of the page tables / top of stack
PM4_START   equ 0x9c000
PDP_START   equ 0x9d000
PD_START    equ 0x9e000
PT_START    equ 0x9f000

PRESENT     equ (1 << 0)
WRITE       equ (1 << 1)
