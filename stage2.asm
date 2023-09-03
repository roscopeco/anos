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
; 
; What it does:
;
;   * Entry in real mode (from stage1)
;     * Prints a message as proof of life (with BIOS routines)
;     * Checks the CPU supports long mode (and dies with a message if not)
;     * Sets up the GDT with both 32- and 64-bit code / data segments
;   * Enables 32-bit protected mode
;     * Sets up data and stack segments for protected mode
;     * Prints another message (direct to VGA mem, because, why not?)
;     * Checks if A20 is enabled, and tries to enable it (keyboard controller method only right now)
;     * Builds a basic page table to identity-map the first 2MB, so we can...
;   * Enable 64-bit long mode
;     * Does a bit more printing (I like printing, okay?)
;     * halts
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

  mov   si,DOT                            ; Print a dot (wastefully)
  call  real_print_sz

  call  guard_long_mode                   ; Now ensure we have a processor with long mode, might as well die early if not...

.protect:
  ; Jump to protected mode
  lgdt  [GDT_DESC]                        ; Load GDT reg with the descriptor

  mov   eax,cr0                           ; Get control register 0 into eax
  or    al,1                              ; Set PR bit (enable protected mode)
  mov   cr0,eax                           ; And put back into cr0

  jmp   0x08:main32                       ; Far jump to main32, use GDT selector at offset 0x08.


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

too_old:
  mov   si,OLD                            ; Load "too old" message
  call  real_print_sz                     ; Print it
.die:
  hlt                                     ; Die...
  jmp   .die                              ; and stay dead

; Print sz string (16-bit)
;
; Arguments:
;   SI - Should point to string
;
; Modifies:
;
;   SI
;
real_print_sz:
  push  ax
  push  bx

  mov   bh,0
  mov   ah,0x0e                           ; Set up for int 10 function 0x0e (TELETYPE OUTPUT)

.charloop:
  lodsb                                   ; get next char (increments si too)
  test  al,al                             ; Is is zero?
  je    .done                             ; We're done if so...
  int   0x10                              ; Otherwise, print it
  jmp   .charloop                         ; And continue testing...

.done:
  pop   bx
  pop   ax
  ret


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 32-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 32
main32:
  mov   ax,0x10                           ; Set ax to 0x10 - offset of segment 2, the 32-bit data segment...
  mov   ds,ax                             ; and set DS to that..
  mov   es,ax                             ; and ES too...
  mov   ss,ax                             ; as well as SS.
  mov   esp,PM4_START                     ; Stack below page tables (16k below top of free low RAM)

  mov   byte [0xb8000],'Y'                ; Print "Y"
  mov   byte [0xb8001],0x1b               ; In color
  mov   byte [0xb8002],'O'                ; Print "O"
  mov   byte [0xb8003],0x1b               ; In color
  mov   byte [0xb8004],'L'                ; Print "L"
  mov   byte [0xb8005],0x1b               ; In color
  mov   byte [0xb8006],'O'                ; Print "O"
  mov   byte [0xb8007],0x1b               ; In color

  call  enable_a20                        ; Enable A20 gate
  call  init_page_tables                  ; Init basic (identity) page tables for long-mode switch
  jmp   go_long

.die:
  hlt
  jmp .die                                ; Just halt for now...


; Enable the A20 line, unless it's already enabled (in which case,
; this is a no-op success).
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
  mov   byte [0xb8000],'D'                ; Print "D"
  mov   byte [0xb8001],0x4c               ; In color

  call  enable_a20_kbd                    ; Try the Keyboard method

  push  eax                               ; Save EAX again

  call  check_a20                         ; Re-check the A20 line

  test  ax,ax                             ; Is AX zero?
  pop   eax                               ; Restore registers
  je    .still_disabled             
  
  mov   byte [0xb8004],'E'                ; Print "E"
  mov   byte [0xb8005],0x2a               ; In color
  ret

.still_disabled:
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
; it though to be fair...
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
  mov   ss,ax                             ; as well as SS.
  mov   rsp,PM4_START                     ; Stack below page tables (16k below top of free low RAM)

  mov   byte [0xb8002],'L'                ; Print "L"
  mov   byte [0xb8003],0x2a               ; In color

.die:
  hlt
  jmp   .die                ; Just die for now...


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

%defstr version_str VERSTR
MSG   db  "ANLOAD #", version_str, 0
OLD   db  13, 10, "Sorry, your CPU is not supported.", 0
GOOD  db  13, 10, 0
DOT   db  ".", 0

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

