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
;     * Searches the FAT for STAGE2.BIN
;       * If found, loads it at 0x9c00 (TODO make this more flexible!)
;       * Otherwise, prints error message and halts
;
; What it doesn't:
;
;   * Support any drive other than zero
;   * Check any file attributes or anything
;   * Much error checking at all really...
;   * Support anything other than FAT12 (currently, _maybe_ FAT16 in future)
; 

%define FAT_FILENAME    0x00
%define FAT_FILEEXT     0x08
%define FAT_ATTRS       0x0b
%define FAT_UNUSED      0x0c
%define FAT_CTIME_MS    0x0d
%define FAT_CTIME_FMT   0x0e
%define FAT_CDATE_FMT   0x10
%define FAT_ADATE_FMT   0x12
%define FAT_EADATE      0x14
%define FAT_MTIME       0x16
%define FAT_MDATE       0x18
%define FAT_CLUSTER     0x1a
%define FAT_FILESIZE    0x1c

global _start

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 16-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 16

_start:
BPB:
  jmp start
  
  ; bpb
  ;
  ; N.B. mtools can change these values, so don't hardcode them
  ; elsewhere - get them from this mem location instead!
  ; 
BPB_PADDING       db  0                 ; Padding byte
BPB_OEMNAME       db  "A N O S "        ; OEM Name
BPB_BYTESPERSECT  dw  512               ; Bytes per sector
BPB_SECTPERCLUST  db  1                 ; Sectors per cluster
BPB_RESERVEDSECT  dw  1                 ; Reserved sectors
BPB_FATCOUNT      db  1                 ; Number of FAT tables
BPB_ROOTENTCOUNT  dw  224               ; Root dir entries
BPB_SECTCOUNT     dw  2880              ; Sector count
BPB_MEDIA_DESC    db  0xf0              ; Media descriptor
BPB_SECTPERFAT    dw  9                 ; Sectors per FAT
BPB_SECTPERTRACK  dw  9                 ; Sectors per track
BPB_HEADS         dw  2                 ; Heads
BPB_HIDDEN        dw  0                 ; Hidden sectors
  
  ; ebpb
BPB_HIDDEN_HI     dw  0                 ; Hidden sectors (hi)
BPB_SECTPERFS     dd  0                 ; Sectors in filesystem
BPB_DRIVENUM      db  0                 ; Logical drive number
BPB_RESERVED      db  0                 ; RESERVED
BPB_EXTSIG        db  0x29              ; Extended signature
BPB_SERIAL        dd  0x12345678        ; Serial number
BPB_VOLNAME       db  "ANOSDISK001"     ; Volume Name
BPB_FSTYPE        db  "FAT12   "        ; Filesystem type
  
start:
  jmp   0x0000:.init                      ; Far jump in case BIOS jumped here via 0x07c0:0000
  mov   [bootDrv],dl                      ; Stash boot drive away for later...

.init:
  xor   ax,ax                             ; Zero ax
  mov   ds,ax                             ; Set up data segment...
  mov   es,ax                             ; ... and extra segment (floppy read uses this)

  mov   ax,0x7fff                         ; Put stack near top of conventional mem for now...
  mov   ss,ax                             ; ...
  mov   sp,0x400                          ; ...

  mov   si,SEARCH_MSG                     ; Load message
  call  print_sz                          ; And print it

.load:
  mov   ax,[BPB_BYTESPERSECT]             ; Bytes per sector in AX
  mov   cx,0x20                           ; Bytes per entry in DX
  div   word cx                           ; Divide to get entries per sector
  mov   bx,ax                             ; BX is now entries per sector

  mov   al,[BPB_FATCOUNT]                 ; Number of FATs in AL
  mul   byte [BPB_SECTPERFAT]             ; Multiply by sectors per FAT
  add   ax,[BPB_RESERVEDSECT]             ; Add reserved sectors; AX is now root dir first sector
  push  ax                                ; Stash it for a sec...

  ; TODO does this scale? Is it safe to load the whole root directory?
  mov   ax,[BPB_ROOTENTCOUNT]             ; Number of root entries in AX
  div   bx                                ; Divide by entries per sector
  xor   cx,cx                             ; Zero cx out...
  mov   cl,al                             ; CX is now number of sectors to load

  pop   ax                                ; Get first sector back into AX

  mov   bx,BUFFER                         ; Buffer location into bx (needs to be at ES:BX)
  call  read_floppy_sectors               ; Read the sectors
  jc    .read_error                       ; Read error if carry flag set

  ; Search the root directory for stage2
  mov   cx,[BPB_ROOTENTCOUNT]             ; Number of root entries in CX
  mov   di,BUFFER                         ; Start at buffer pointer

  mov   bh,0                              ; Set up a dot for printing
  mov   ah,0x0e
  mov   al,'.'                            

.search:
  int   0x10                              ; Print a dot
  push  cx                                ; Stash counter
  mov   cx,11                             ; Names are 11 bytes
  mov   si,STAGE_2                        ; Comparing with our expected name...
  push  di                                ; rep will change DI
  rep cmpsb                               ; Compare strings...
  pop   di                                ; Restore DI
  je    .load_stage2                      ; Match - jump to finding the starting cluster
  pop   cx                                ; Else, restore counter...
  add   di,0x20                           ; ... advance DI 32-bytes to next filename...
  loop  .search                           ; ... and loop

  mov   si,NO_FILE                        ; If we're out of entries, load error message
  jmp   .show_error                       ; Show it, then die...

.load_stage2:
  mov   si,CRLF                           ; Print loading message...
  call  print_sz

  mov   dx,[di + FAT_CLUSTER]             ; Get stage2 starting cluster

.load_fat:
  push  dx                                ; Stash starting cluster

  mov   bx,BUFFER                         ; Loading FAT into the buffer
  mov   ax,[BPB_RESERVEDSECT]             ; FAT starts after reserved sectors...
  mov   cx,[BPB_SECTPERFAT]               ; Load the entire FAT
  mov   dl,[bootDrv]                      ; Load the boot drive...

  call  read_floppy_sectors               ; Read in the FAT
  jc    .read_error                       ; Error and die if read failed

  cmp   al,cl                             ; Did we read the right number?
  jne   .read_error                       ; Error and die if not

  mov   si,bx                             ; Move BUFFER ptr into SI

  ; TODO Fixed load position (currently at 0x9c00)
  ;
  mov   bx,STAGE_2_ADDR                   ; Set BX up to load stage 2 at 0x9c00
  pop   dx                                ; Get starting cluster back into DX

.load_loop:
  mov   ax,dx                             ; Current cluster number into AX
  push  dx                                ; Stash DX again (TODO find a more efficient way than this)

  sub   ax,0x02                           ; Subtract 2
  xor   cx,cx                             ; Clear CX
  mov   cl,byte [BPB_SECTPERCLUST]        ; Get sectors per cluster...  
  mul   cx                                ; And multiply; AX now has LBA sector number                                         
                                          ; and CX has the number of sectors per cluster.

  push  ax                                ; Save AX for a sec
  mov   ax,[BPB_SECTPERFAT]               ; and load sectors per FAT into it
  mov   dl,byte [BPB_FATCOUNT]            ; Get FAT count into DL
  mul   dl                                ; Multiply to get total FAT sectors
  mov   dx,[BPB_RESERVEDSECT]             ; Get number of reserved sectors
  add   dx,ax                             ; Add to get start of data section; now in DX
  pop   ax                                ; Retrieve relative sector number into AX
  add   ax,dx                             ; And add to make absolute sector number in AX

  push  cx                                ; Stash CX 
  push  ax                                ; And stash AX again (and once more, TODO clean this up!)
  mov   ax,[BPB_BYTESPERSECT]             ; Get bytes per sector into AX
  mov   cx,0x20                           ; Root entry size into CX
  xor   dx,dx                             ; Zero out dx
  div   cx                                ; Divide to get root entries per sector

  mov   cx,ax                             ; Root entries per sector into CX
  mov   ax,[BPB_ROOTENTCOUNT]             ; Total root entries into AX
  xor   dx,dx                             ; Zero out dx again
  div   cx                                ; Divide total root entries by entries per sector for root directory sector count

  sub   cx,0x2                            ; TODO FIGURE OUT WHY THIS IS OFF BY TWO!!!!!1!

  pop   ax                                ; Get sector number back into AX
  add   ax,cx                             ; Add root dir sector count

  pop   cx                                ; And restore sectors per cluster into CX

  mov   dl,[bootDrv]                      ; Load the boot drive...

  call  read_floppy_sectors               ; Read in this cluster

  pop   dx                                ; Get current cluster back into DX (again 🙄)
  call  next_cluster                      ; Else, replace dx with next cluster...

  cmp   dx,0xFFF                          ; Is this the last cluster?
  je    .load_done                        ; Done if so

  add   bx,0x200                          ; Else move buffer pointer to start of next sector...
  jmp   .load_loop                        ; ... and loop

.load_done:
  mov   bx,STAGE_2_ADDR                   ; Go for stage 2...
  jmp   bx

.read_error:
  mov   si,READ_ERROR                     ; Load message...

.show_error:
  push  si
  mov   si,CRLF
  call  print_sz
  pop   si
  call  print_sz                          ; And print it

.die:
  cli
  hlt
  jmp .die


; Get next cluster in FAT chain
;
; Arguments:
;   DX - Current cluster
;
; Modifies:
;   Nothing
;
next_cluster:
  push  ax                                ; Save registers
  push  bx
  push  cx

  mov   ax,dx                             ; Copy current cluster
  mov   cx,dx                             ; Twice...
  shr   dx,0x01                           ; Divide cluster by 2
  add   cx,dx                             ; And add it to get byte offset in FAT into CX

  mov   bx,BUFFER                         ; FAT buffer pointer in BX
  add   bx,cx                             ; Offset to first FAT byte for cluster
  mov   dx,word [bx]                      ; Fetch a word

  test  ax,0x01                           ; Is it odd?
  jz    .even                             ; Nope, go deal with that...

.odd:
  shr   dx,0x4                            ; Else, yes! Discard low four bits
  jmp   .done

.even:
  and   dx,0x0FFF                         ; Even - Mask off high four bits

.done:
  pop   cx                                ; Restore regs
  pop   bx
  pop   ax
  ret                                     ; And we're done


; Reset floppy drive.
;
; On return, if carry is set, the operation failed...
;
reset_floppy:
  push  dx                               ; Save registers
  push  cx
  push  ax

  mov   cl,0x04                           ; three tries (we decrement first, so +1)

.reset:
  dec   cl                                ; Decrement cl, without changing carry
  jz    .done                             ; Tries exhausted if zero

  mov   ah,0                              ; Function 0
  mov   dl,0                              ; Drive zero (TODO hardcoded!)
  int   0x13                              ; Int 0x13 for disks
  jc    .reset                            ; If carry, error, so loop and try again...

.done:
  pop   ax                                ; Restore regs
  pop   cx
  pop   dx
  ret


; Read floppy sectors
;
; Arguments:
;   AX - Sector num
;   CL - Sector count
;   DL - Drive
;   ES:BX - Buffer
;
; Modifies:
;   AH - Status code
;   AL - Sector count
;   CF - Set on error (AX values are not valid in this case!)
;
read_floppy_sectors:
  push  dx                                ; Save regs
  push  cx

  call  reset_floppy                      ; Try to reset
  jc    .done                             ; If we can't reset, just give up

  push  cx
  call  lba_to_chs                        ; Convert LBA sector in AX to CHS in correct registers for BIOS
  pop   ax

  mov   ah,0x04                           ; three tries (we decrement first, so +1)

.read:
  dec   ah                                ; Decrement tries, without changing carry
  jz    .done                             ; Tries exhausted if zero
  
  push  ax                                ; Save current tries count
  mov   ah,0x02                           ; Function 2

  int   0x13                              ; Int 0x13 for disks

  mov   [.result],ax                      ; Save result for a sec
  pop   ax                                ; Tries count back into ah
  jc    .read                             ; If carry, error, so loop and try again...

.done:
  pop   cx                                ; Restore regs
  pop   dx                          
  mov   ax,[.result]                      ; Restore result and done
  ret
.result   dw  0


; Convert LBA to CHS for floppy (only!)
; 
; Arguments:
;
;   AX - LBA sector num
;
; Modifies:
;
;   CH - Track number (low order 8 bits)
;   CL - Sector number (max 5 bits)
;   DH - Head number
; 
lba_to_chs:
  push  ax                                ; Save regs
  push  bx

  mov   bx,dx                             ; Stash DX (for drive number) in BX

  xor   dx,dx                             ; Zero DX for div
  div   word [BPB_SECTPERTRACK]           ; divide by sectors per track
  inc   dl                                ; add 1 for 1-based sector
  mov   cl,dl                             ; CL is now sector number

  xor   dx,dx                             ; Zero DX again for another div
  div   word [BPB_HEADS]                  ; mod previous result by number of heads
  mov   dh,dl                             ; Remainder of that is head number
  mov   ch,al                             ; And result is cylinder number

  mov   dl,bl                             ; Put drive number back into DL

  pop   bx                                ; Restore regs
  pop   ax
  ret


; Print sz string
;
; Arguments:
;   SI - Should point to string
;
; Modifies:
;
;   SI
;
print_sz:
  push  ax
  push  bx

  mov   bh,0
  mov   ah,0x0e                           ; Set up for int 10 function 0x0e (TELETYPE OUTPUT)

.charloop:
  lodsb                                   ; get next char (increments si too)
  cmp   al,0                              ; Is is zero?
  je    .done                             ; We're done if so...
  int   0x10                              ; Otherwise, print it
  jmp   .charloop                         ; And continue testing...

.done:
  pop   bx
  pop   ax
  ret


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%defstr version_str VERSTR
SEARCH_MSG  db "ANBOOT #",version_str, 0
NO_FILE     db "E:NF", 10, 13, 0
READ_ERROR  db "E:RE", 10, 13, 0
CRLF        db 10, 13, 0
STAGE_2     db "STAGE2  BIN"

bootDrv     db  0

align 2

BUFFER:
times 510-($-$$) nop
dw  0xaa55

