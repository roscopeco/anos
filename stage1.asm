bits 16
org 0x7c00
bootsect:
  jmp start
  
  ; bpb
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
  xor di,di
  mov si,MSG
  mov ah,0x0e

.charloop:
  lodsb
  cmp al,0
  je  .die
  int 0x10
  jmp .charloop

.die:
  cli
  hlt
  jmp .die

MSG db "No bootable device.", 10, 13, 0

  times 510-($-$$) nop
  dw  0xaa55

