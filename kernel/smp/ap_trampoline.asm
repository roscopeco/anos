bits 16

section .text.init
global _start

; This is the AP trampoline...
_start:
  cli
  cld
  mov [cs:0x10],byte $1
  
.die:
    hlt
    jmp .die


align 16

AP_GDT:
  ; segment 0 - null
  dq 0

  ; segment 1 - 64-bit code
  dw 0                    ; limit ignored for long mode
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b10100000           ; Flags + Limit: 1 = 4k granularity, 0 = 16-bit, 1 = Long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 2 - 64-bit data
  dw 0                    ; limit (ignored in 64-bit mode)
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b00100000           ; Flags + Limit: 0 = 1-byte granularity, 0 = 16-bit, 1 = Long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 3 - 32-bit code
  dw 0xFFFF               ; Limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10011010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 1 = Executable, 0 = Nonconforming, 1 = Readable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 4 - 32-bit data
  dw 0xFFFF               ; Limit 4GB
  dw 0                    ; Base (bits 0-15) - 0
  db 0                    ; Base (bits 16-23) - 0
  db 0b10010010           ; Access: 1 = Present, 00 = Ring 0, 1 = Type (non-system), 0 = Non-Executable, 0 = Grows up, 1 = Writeable, 0 = Accessed
  db 0b11001111           ; Flags + Limit: 1 = 4k granularity, 1 = 32-bit, 0 = Non-long mode, 0 = reserved (for our use)
  db 0                    ; Base (bits 23-31) - 0

  ; segment 5 - TSS - Base is calculated in code...
  dw 0x0067               ; 104 bytes for a TSS
AP_TSS_BASE_LOW:
  dw 0                    ; Base (bits 0-15) - 0 (calculated at runtime)
AP_TSS_BASE_MID:
  db 0                    ; Base (bits 16-23) - 0 (calculated at runtime)
  db 0b10001001           ; Access: 1 = Present, 00 = Ring 0, 0 = Type (system), 1001 = Long mode TSS (Available)
  db 0b00010000           ; Flags + Limit: 0 = byte granularity, 0 = 16-bit, 0 = Long mode, 1 = Available
AP_TSS_BASE_HIGH:
  db 0                    ; Base (bits 23-31) - 0 (calculated at runtime)
  dd 0xFFFFFFFF           ; Base (bits 32-63) - 0xFFFFFFFF (in the identity-mapped kernel mem)
  dd 0                    ; Reserved

AP_GDT_DESC:
  ; GDT Descriptor
  dw  AP_GDT_DESC-AP_GDT-1; Size (computed from here - start)
  dd  AP_GDT              ; Address (GDT, above)


AP_TSS:
  dd  0                   ; Reserved
  dq  0xFFFFFFFF80110000  ; RSP0
  dq  0                   ; RSP1
  dq  0                   ; RSP2
  dq  0                   ; Reserved
  dq  0                   ; IST1
  dq  0                   ; IST2
  dq  0                   ; IST3
  dq  0                   ; IST4
  dq  0                   ; IST5
  dq  0                   ; IST6
  dq  0                   ; IST7
  dq  0                   ; IST8
  dw  0                   ; Reserved
  dw  0                   ; IOPB
