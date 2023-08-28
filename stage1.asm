; stage1 - Boot sector stage 1 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Totally for fun and learning, it should work but it's likely wrong
; in many and subtle ways...
;
bits 16

global _start

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
  xor ax,ax               ; Zero ax
  mov ds,ax               ; Set up data segments...

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
  cli                     ; No interrupts (permadeath)
  xor ax,ax               ; zero ax...
  mov ds,ax               ; and set DS to that zero  

  lgdt [GDT_DESC]         ; Load GDT reg with the descriptor

  mov eax,cr0             ; Get control register 0 into eax
  or  al,1                ; Set PR bit (enable protected mode)
  mov cr0,eax             ; And put back into cr0

  jmp 0x08:main           ; Far jump to main, use GDT selector at offset 0x08.

bits 32
main:
  mov ax,0x10             ; Set ax to 0x10 - offset of segment 2, the data segment...
  mov ds,ax               ; and set DS to that..
  mov es,ax               ; and ES too...
  mov ss,ax               ; as well as SS.
  mov esp,0x9FFFF         ; Stack at top of free memory for now...

  mov byte [0xb8000],'Y'  ; Print "Y"
  mov byte [0xb8001],0x1b ; In color
  mov byte [0xb8002],'O'  ; Print "O"
  mov byte [0xb8003],0x1b ; In color
  mov byte [0xb8004],'L'  ; Print "L"
  mov byte [0xb8005],0x1b ; In color
  mov byte [0xb8006],'O'  ; Print "O"
  mov byte [0xb8007],0x1b ; In color
  
  call enable_a20

  ; TODO enable paging and set up long-mode

.die:
  cli
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
  mov byte [0xb8001],0xaa ; In color
  ret

.disabled:
  mov byte [0xb8000],'D'  ; Print "D"
  mov byte [0xb8001],0xcc ; In color, flash

  ; TODO actually enable, if not already!
.die
  cli
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


MSG db "Stage 1 starting up...", 10, 13, 0
DIS db "A20 is disabled", 10, 13, 0
EN  db "A20 is enabled", 10, 13, 0

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

GDT_DESC:
  ; GDT Descriptor
  dw  GDT_DESC-GDT        ; Size (computed from here - start)
  dd  GDT                 ; Address (GDT, above)

  times 510-($-$$) nop
  dw  0xaa55

