; stage3 - 8259 PIC routines
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; I'm not using the PIC, so this just contains code to disaale it.
;

global pic_init
bits 64

%define PIC1_BASE       0x20              ; PIC 1 base port
%define PIC2_BASE       0xA0              ; PIC 2 base port
%define PIC1_COMMAND    PIC1_BASE         ; PIC 1 command port
%define PIC2_COMMAND    PIC2_BASE         ; PIC 2 command port
%define PIC1_DATA       PIC1_BASE+1       ; PIC 1 data port
%define PIC2_DATA       PIC2_BASE+1       ; PIC 2 data port

%define ICW1_ICW4       0x01              ; ICW4 present
%define ICW1_INIT       0x10              ; Initialization
%define ICW4_8086       0x01              ; 8086/8088 mode


pic_init:
  push  rax                               ; Save registers

  ; Still need to remap the PICs in case of spurious interrupts...
  mov   al,ICW1_INIT | ICW1_ICW4          ; ICW1: Initialize, 8086 mode
  out   PIC1_COMMAND,al
  call  .wait
  out   PIC2_COMMAND,al                   ;       For both PICs,al
  call  .wait

  mov   al,0x20                           ; ICW2: Vector base PIC1 to 0x20
  out   PIC1_DATA,al
  call  .wait

  mov   al,0x28                           ;       Vector base PIC2 to 0x28
  out   PIC2_DATA,al
  call  .wait

  mov   al,0x04                           ; ICW3: Configure master with slave at IRQ2
  out   PIC1_DATA,al
  call  .wait

  mov   al,0x02                           ;       Configure slave cascade identity (i.e. IRQ2)
  out   PIC2_DATA,al
  call  .wait

  mov   al,ICW4_8086                      ; ICW4: Configure both PICs for 8086 mode
  out   PIC1_DATA,al
  call  .wait
  out   PIC2_DATA,al
  call  .wait

  mov   al,0xff                           ; Now, disable the PICs
  out   PIC1_DATA,al
  call  .wait
  out   PIC2_DATA,al

  pop   rax                               ; Restore registers
  ret

.wait:
  out   0x80,al                           ; Output whatever's in AL to port 80
  ret                                     ; (just waste a few usec, _maybe_ not needed on modern machines, but 🤷)



  
  
