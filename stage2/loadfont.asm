; stage2 - 8x8 font loader & mode setup
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;
; This is kinda silly, looks amateurish, and 
; probably won't get kept. 
;
; Was a fun hack though :D 
;

bits 16

global load_font

section .rodata
%include "stage2/bootlogo_spritesheet_8x8_Sheet.inc"

section .text

load_font:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax          ; ES:BP will point to font data
    mov bp, font_data   ; BP points to our font data

    ; First load the ROM 8x8 font
    mov ax, 1112h       ; AH=11h: Character generator, AL=12h: Load ROM 8x8 font
    mov bl, 00h         ; ROM 8x8 first font
    int 10h

    ; INT 10h, AH=11h - Character Generator Routines
    mov ax, 1110h       ; AL=10h: Load user-specified patterns
    mov bx, 0800h       ; BH=08h: 8 bytes per character pattern
                        ; BL=00h: Block to load
    mov cx, NUM_CHARS   ; Number of characters to load
    mov dx, 0x80        ; Starting character (first extended ASCII)
    int 10h

    mov dx, 0x3C4       ; VGA Sequencer index port
    mov al, 0x01        ; Clocking Mode register
    out dx, al
    inc dx              ; Move to data port (0x3C5)
    in  al, dx          ; Read current value
    or  al, 0b00000001  ; Set bit 0 (8 dot width)
    out dx, al          ; Write it back

    mov dx, 0x3C2       ; Set clock reg select to 25MHz,
    xor al,al           ; (3C2, 3:0 = 0)
    out dx, al          ; I have no idea if this will work...


    ret
