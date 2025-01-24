; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; A20 gate checker / enabler (keyboard controller only at present)

bits 32
global check_a20, enable_a20

; Enable the A20 line, unless it's already enabled (in which case,
; this is a no-op success).
;
; This should rarely, if ever, do anything. On a modern x86_64 bit
; machine (like we know we have, since we check at the beginning
; of stage 2) A20 might not even be a thing, and if it is, the 
; BIOS call we did before leaving real mode should reliably
; sort it out, meaning this should just check, find it's okay,
; and return without doing much...
;
; Call in protected mode.
;
; Modifies: nothing
;
enable_a20:
  push  eax                               ; Save registers

  call  check_a20                         ; Check the A20 line

  test  ax,ax                             ; Is AX zero?
  pop   eax                               ; Restore registers
  je    .disabled

  mov   byte [0xb8000],'E'                ; Print "E"
  mov   byte [0xb8001],0x2a               ; In color
  ret

.disabled:
  in al, 0x92                             ; Try the fast method...
  or al, 2
  out 0x92, al

  call  check_a20                         ; Check the A20 line again

  test  ax,ax                             ; Is AX zero?
  pop   eax                               ; Restore registers
  je    .still_disabled

  mov   byte [0xb8004],'E'                ; Print "E"
  mov   byte [0xb8005],0x2a               ; In color
  ret

.still_disabled:
  ret
  mov   byte [0xb8000],'D'                ; Print "D"
  mov   byte [0xb8001],0x4c               ; In color
  call  enable_a20_kbd                    ; Try the Keyboard method

  push  eax                               ; Save EAX again

  call  check_a20                         ; Re-check the A20 line

  test  ax,ax                             ; Is AX zero?
  pop   eax                               ; Restore registers
  je    .really_disabled
  
  mov   byte [0xb8004],'E'                ; Print "E"
  mov   byte [0xb8005],0x2a               ; In color
  ret

.really_disabled:
  ; TODO just die for now - but we should try BIOS and fast methods too...
  mov   byte [0xb8004],'X'                ; Print "X"
  mov   byte [0xb8005],0x4c               ; In color

.die:
  hlt
  jmp   .die                              ; Don't enable, just die for now...
  

; Enable the A20 line with the keyboard controller method. 
; This is the traditional (i.e. "old") method and _might_ be
; supported everywhere(?).
;
; I wouldn't be surprised some modern computers don't support
; it though to be fair... [edit: Virtualbox doesn't either!]
;
; Port 0x64 is the keyboard controller command port. Bits:
;
; Status Register - PS/2 Controller
; Bit Meaning
; 0   Output buffer status (0 = empty, 1 = full) (must be set before attempting to read data from IO port 0x60)
; 1   Input buffer status (0 = empty, 1 = full) (must be clear before attempting to write data to IO port 0x60 or IO port 0x64)
; 2   System Flag - Meant to be cleared on reset and set by firmware (via. PS/2 Controller Configuration Byte) if the system passes self tests (POST)
; 3   Command/data (0 = data written to input buffer is data for PS/2 device, 1 = data written to input buffer is data for PS/2 controller command)
; 4   Unknown (chipset specific) - May be "keyboard lock" (more likely unused on modern systems)
; 5   Unknown (chipset specific) - May be "receive time-out" or "second PS/2 port output buffer full"
; 6   Time-out error (0 = no error, 1 = time-out error)
; 7   Parity error (0 = no error, 1 = parity error)
;
; (from https://stackoverflow.com/questions/21078932/why-test-port-0x64-in-a-bootloader-before-switching-into-protected-mode)
;
; Assumes: 32-bit pmode, interrupts disabled
; Modifies: nothing
;
enable_a20_kbd:
  push  eax                             ; Save registers

  call  .wait_a20_write                 ; Make sure controller is ready for commands...
  mov   al,0xad                         ; Output 0xad (DISABLE KEYBOARD) to the command port
  out   0x64,al

  call  .wait_a20_write                 ; Again, wait for it to be ready.
  mov   al,0xd0                         ; Output 0xd0 (READ OUTPUT) to command port
  out   0x64,al                       

  call  .wait_a20_read                  ; ... so do the check
  in    al,0x60                         ; Read output port command into AL
  push  eax                             ; And stash it away for a bit while setting up a write

  call  .wait_a20_write                      
  mov   al,0xD1                         ; Output 0xd1 (WRITE OUTPUT) command to command port
  out   0x64,al

  call  .wait_a20_write                 ; Again, wait until we can write
  pop   eax                             ; Restore the value we got from the output port above
  or    al,0x2                          ; Set bit 1 (A20 Gate)
  out   0x60,al                         ; And write it to the output port

  call  .wait_a20_write                 ; Wait before the final command
  mov   al,0xae                         ; Output 0xae (ENABLE KEYBOARD) to command port
  out   0x64,al

  call  .wait_a20_write                 ; Wait one last time to be "sure" it's finished...
  pop   eax                             ; Restore registers and done
  ret

.wait_a20_write:
  in    al,0x64                         ; Read port 0x64
  test  al,0x02                         ; Test bit 1 (input buffer status)
  jnz   .wait_a20_write                 ; If it's set, just busywait
  ret

.wait_a20_read:
  in    al,0x64                         ; Read port 0x64
  test  al,0x01                         ; Test bit 0 (output buffer status)
  jz    .wait_a20_read                  ; If it's clear, just busywait
  ret


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
  push  esi                               ; Save registers
  push  edi

  mov   esi,0x007c07                      ; Use the second half of our OEM signature for the check
  mov   edi,0x107c07                      ; Address 1MB above

  mov   [edi],dword 0x20532055            ; Set different value at higher address

  cmpsd                                   ; Compare them

  pop   edi                               ; Restore registers
  pop   esi

  jne   .is_set
  xor   ax,ax                             ; A20 disabled, so clear ax
  ret

.is_set:
  mov   ax,1                              ; A20 enabled, so set ax
  ret
