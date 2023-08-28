; stage1 - Boot sector stage 1 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Totally for fun and learning, it should work but it's likely wrong
; in many and subtle ways...
;
; What it does:
;
;   * Entry in real mode (from the bios)
;     * Prints a message as proof of life (with BIOS routines)
;     * Sets up the GDT with both 32- and 64-bit code / data segments
;   * Enables 32-bit protected mode
;     * Sets up data and stack segments for protected mode
;     * Prints another message (direct to VGA mem, because, why not?)
;     * Checks if A20 is enabled, and dies if not (enabling is still TODO!)
;     * Builds a basic page table to identity-map the first 2MB, so we can...
;   * Enable 64-bit long mode
;     * Does a bit more printing (I like printing, okay?)
;     * halts
;
; What it doesn't:
;
;   * Enable A20 - both emulators I'm using enable it anyway
;   * Disable IRQ or NMI - works for now on the emulators, but will need it later
;   * Set up IDT - Didn't want to bother in 32-bit mode, I'll take the triple-faults...
;
; It's pretty full (there are like 5 bytes left) so I'll probably split most of
; this out as stage2 next, and just have this stage1 load that stage2 and go from
; there...
; 

global _start

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 16-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 16

_start:
BPB:
  jmp start
  
  ; bpb
  db  0                                 ; Padding byte
  db  "A N O S "                        ; OEM Name
  dw  512                               ; Bytes per sector
  db  1                                 ; Sectors per cluster
  dw  1                                 ; Reserved sectors
  db  1                                 ; Number of FAT tables
  dw  224                               ; Root dir entries
  dw  2880                              ; Sector count
  db  0xf0                              ; Media descriptor
  dw  9                                 ; Sectors per FAT
  dw  9                                 ; Sectors per track
  dw  2                                 ; Heads
  dw  0                                 ; Hidden sectors
  
  ; ebpb
  dw  0                                 ; Hidden sectors (hi)
  dd  0                                 ; Sectors in filesystem
  db  0                                 ; Logical drive number
  db  0                                 ; RESERVED
  db  0x29                              ; Extended signature
  dd  0x12345678                        ; Serial number
  db  "ANOS-DISK01"                     ; Volume Name
  db  "FAT12"                           ; Filesystem type
  
start:
  jmp 0x0000:.init        ; Far jump in case BIOS jumped here via 0x07c0:0000

.init:
  xor ax,ax               ; Zero ax
  mov ds,ax               ; Set up data segment...

  mov si,MSG              ; Load message
  mov ah,0x0e             ; Set up for int 10 function 0x0e (TELETYPE OUTPUT) 

.charloop:
  lodsb                   ; get next char (increments si too)
  cmp al,0                ; Is is zero?
  je  .protect            ; We're done if so...
  int 0x10                ; Otherwise, print it
  jmp .charloop           ; And continue testing...

.protect:
  ; Jump to protected mode
  cli                     ; No interrupts
  xor ax,ax               ; zero ax...
  mov ds,ax               ; and set DS to that zero  

  lgdt [GDT_DESC]         ; Load GDT reg with the descriptor

  mov eax,cr0             ; Get control register 0 into eax
  or  al,1                ; Set PR bit (enable protected mode)
  mov cr0,eax             ; And put back into cr0

  jmp 0x08:main32         ; Far jump to main32, use GDT selector at offset 0x08.


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 32-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 32
main32:
  mov ax,0x10             ; Set ax to 0x10 - offset of segment 2, the 32-bit data segment...
  mov ds,ax               ; and set DS to that..
  mov es,ax               ; and ES too...
  mov ss,ax               ; as well as SS.
  mov esp,PM4_START       ; Stack below page tables (16k below top of free low RAM)

  mov byte [0xb8000],'Y'  ; Print "Y"
  mov byte [0xb8001],0x1b ; In color
  mov byte [0xb8002],'O'  ; Print "O"
  mov byte [0xb8003],0x1b ; In color
  mov byte [0xb8004],'L'  ; Print "L"
  mov byte [0xb8005],0x1b ; In color
  mov byte [0xb8006],'O'  ; Print "O"
  mov byte [0xb8007],0x1b ; In color
  
  call enable_a20         ; Enable A20 gate
  call init_page_tables   ; Init basic (identity) page tables for long-mode switch
  jmp go_long

.die:
  hlt
  jmp .die                ; Just halt for now...


; Enable the A20 line, unless it's already enabled (in which case,
; this is a no-op success).
;
; Call in protected mode.
;
; Modifies: nothing
;
enable_a20:
  push eax                ; Save registers

  call check_a20          ; Check the A20 line

  cmp ax,0                ; Is AX zero?
  pop eax                 ; Restore registers
  je  .disabled             

  mov byte [0xb8000],'E'  ; Print "E"
  mov byte [0xb8001],0x2a ; In color
  ret

.disabled:
  mov byte [0xb8000],'D'  ; Print "D"
  mov byte [0xb8001],0x4c ; In color

  ; TODO actually enable, if not already!
.die:
  hlt
  jmp .die                ; Don't enable, just die for now...
  

; Is the A20 line enabled?
;
; Call in protected mode.
;
; Returns: ax = 0 if the A20 line is disabled
;          ax = 1 if the A20 line is enabled
;
; Modifies: ax
;
check_a20:
  push esi                ; Save registers
  push edi

  mov esi,0x007c07        ; Use the second half of our OEM signature for the check
  mov edi,0x107c07        ; Address 1MB above

  mov [edi],dword 0x20532055    ; Set different value at higher address

  cmpsd                   ; Compare them

  pop edi                 ; Restore registers
  pop esi

  jne .is_set
  xor ax,ax               ; A20 disabled, so clear ax
  ret

.is_set:
  mov ax,1                ; A20 enabled, so set ax
  ret


; Initialise the initial page tables ready for long mode. Just identity maps
; the bottom 2MB for the switch - proper tables will be set up later...
;
; Call in protected mode.
;
; Modifies: Nothing
init_page_tables:
  push eax                ; Save registers
  push ecx
  push edi

  ; Zero out the page table memory
  mov eax,0               ; Value is zero
  mov ecx,0x4000 / 4      ; Count is number of dwords
  mov edi,PM4_START       ; Start at table start
  rep stosd               ; Zero

  ; Set up a single PM4 entry, pointing to the PDP
  mov eax,PDP_START | PRESENT | WRITE
  mov [PM4_START], eax

  ; Set up a single PDPT entry, pointing to the PD
  mov eax,PD_START | PRESENT | WRITE
  mov [PDP_START], eax

  ; Set up a single PD entry, pointing to PT
  mov eax,PT_START | PRESENT | WRITE
  mov [PD_START], eax

  ; Set up the page table to map the first 2MB
  mov eax,PRESENT | WRITE ; Start at addr 0, PRESENT | WRITE
  mov edi,PT_START        ; Filling the PT from the beginning

.pt_loop: 
  mov [edi],eax           ; Move current eax (address and flags) into the current PTE
  add eax,0x1000          ; Add 4K to eax, to point to the next table
  add edi,8               ; Move pointer to next PTE
  cmp eax, 0x200000       ; Have we done 2MB?
  jb  .pt_loop            ; Keep looping if not...

  pop edi                 ; Restore registers and done
  pop ecx
  pop eax
  ret


; Go long mode. This jumps to main64 at the end, so it's noreturn.
; Just jump to it rather than calling it :) 
go_long:  
  mov eax,0b10100000      ; Set PAE and PGE bits
  mov cr4,eax

  mov eax,PM4_START       ; Load PML3 into cr3
  mov cr3,eax

  mov ecx,0xC0000080      ; Read EFER MSR
  rdmsr

  or eax,0x100            ; Set the long-mode enable bit
  wrmsr                   ; and write back to EFER MSR

  mov eax,cr0             ; Activate paging to go full long mode
  or  eax,0x80000000      ; (we're already in protected mode)
  mov cr0,eax

  jmp 0x18:main64         ; Go go go


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 64-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 64
main64:
  mov ax,0x20             ; Set ax to 0x20 - offset of segment 4, the 64-bit data segment...
  mov ds,ax               ; and set DS to that..
  mov es,ax               ; and ES too...
  mov ss,ax               ; as well as SS.
  mov rsp,PM4_START       ; Stack below page tables (16k below top of free low RAM)

  mov byte [0xb8002],'L'  ; Print "L"
  mov byte [0xb8003],0x2a ; In color

.die:
  hlt
  jmp .die                ; Just die for now...


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

MSG db "Stage 1 starting up...", 10, 13, 0

GDT:
  ; segment 0 - null
  dq 0

  ; segment 1 - 32-bit code
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 15-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 2 - 32-bit data
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 15-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 1 - 64-bit code
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 15-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b10101111           ; Flags + Limit: 1 = 4k granularity, 0 = 16-bit, 1 = Long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 2 - 64-bit data
  dw 0xFFFF               ; limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 15-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b10101111           ; Flags + Limit: 1 = 4k granularity, 0 = 16-bit, 1 = Llong mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

GDT_DESC:
  ; GDT Descriptor
  dw  GDT_DESC-GDT-1      ; Size (computed from here - start)
  dd  GDT                 ; Address (GDT, above)

  times 510-($-$$) nop
  dw  0xaa55

