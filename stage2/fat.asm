; stage2 - Boot sector stage 2 loader
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Unreal mode FAT loader for stage3 (the kernel)
;
; This is largely copied verbatim from stage1, maybe there's
; an opportunity to share some code (though this is assembled
; 32-bit, for running in unreal mode, so would want to include
; rather than just link I guess).

bits 16

global load_stage3

extern real_print_sz                      ; Defined in prints.asm

%define FAT_FILENAME      0x00            ; Offset to filename (8 bytes)
%define FAT_FILEEXT       0x08            ; Offset to extension (3 bytes)
%define FAT_ATTRS         0x0b            ; Offset to attribute byte
%define FAT_UNUSED        0x0c            ; Offset to unused byte
%define FAT_CTIME_MS      0x0d            ; Offset to create time MS byte
%define FAT_CTIME_FMT     0x0e            ; Offset to create time word
%define FAT_CDATE_FMT     0x10            ; Offset to create date word
%define FAT_ADATE_FMT     0x12            ; Offset to access date word
%define FAT_EADATE        0x14            ; Offset to ext access date word
%define FAT_MTIME         0x16            ; Offset to modified time word
%define FAT_MDATE         0x18            ; Offset to modified date word
%define FAT_CLUSTER       0x1a            ; Offset to start cluster word
%define FAT_FILESIZE      0x1c            ; Offset to filesize (5 bytes?)

%define BPB_PADDING       0x7c00          ; Padding byte
%define BPB_OEMNAME       0x7c03          ; OEM Name
%define BPB_BYTESPERSECT  0x7c0B          ; Bytes per sector
%define BPB_SECTPERCLUST  0x7c0D          ; Sectors per cluster
%define BPB_RESERVEDSECT  0x7c0E          ; Reserved sectors
%define BPB_FATCOUNT      0x7c10          ; Number of FAT tables
%define BPB_ROOTENTCOUNT  0x7c11          ; Root dir entries
%define BPB_SECTCOUNT     0x7c13          ; Sector count
%define BPB_MEDIA_DESC    0x7c15          ; Media descriptor
%define BPB_SECTPERFAT    0x7c16          ; Sectors per FAT
%define BPB_SECTPERTRACK  0x7c18          ; Sectors per track
%define BPB_HEADS         0x7c1a          ; Heads
%define BPB_HIDDEN        0x7c1c          ; Hidden sectors
%define BPB_HIDDEN_HI     0x7c1e          ; Hidden sectors (hi)
%define BPB_SECTPERFS     0x7c20          ; Sectors in filesystem
%define BPB_DRIVENUM      0x7c24          ; Logical drive number
%define BPB_RESERVED      0x7c25          ; RESERVED
%define BPB_EXTSIG        0x7c26          ; Extended signature
%define BPB_SERIAL        0x7c27          ; Serial number
%define BPB_VOLNAME       0x7c2b          ; Volume Name
%define BPB_FSTYPE        0x7c36          ; Filesystem type

%define DATA_BUFFER       0x7400          ; Data buffer, 2048 bytes below stage 1
                                          ; N.B. This makes max sectors / cluster 4!
                                          ; TODO is this ever the case? Should we check and die if it is?

%define BUFFER            0x7e00          ; Buffer as it was before, at end of loaded boot sector

; Load KERNEL.BIN from disk at the 1MB mark (well, just above).
; TODO could parameterise this in stage 1 and then use it from there,
; it's still loaded anyway when we call this (since we use the BPB).
;
; Modifies: Everything (trashes GP whatever registers)
;
load_stage3:
  mov   ax,[BPB_BYTESPERSECT]             ; Bytes per sector in AX
  mov   cx,0x20                           ; Bytes per entry in DX
  xor   dx,dx                             ; Zero DX before dividing...
  div   word cx                           ; Divide to get entries per sector
  mov   bx,ax                             ; BX is now entries per sector

  mov   al,[BPB_FATCOUNT]                 ; Number of FATs in AL
  mul   byte [BPB_SECTPERFAT]             ; Multiply by sectors per FAT
  add   ax,[BPB_RESERVEDSECT]             ; Add reserved sectors; AX is now root dir first sector
  push  ax                                ; Stash it for a sec...

  ; TODO does this scale? Is it safe to load the whole root directory?
  mov   ax,[BPB_ROOTENTCOUNT]             ; Number of root entries in AX
  xor   dx,dx                             ; Zero DX before dividing again...
  div   bx                                ; Divide by entries per sector
  xor   cx,cx                             ; Zero cx out...
  mov   cl,al                             ; CX is now number of sectors to load

  pop   ax                                ; Get first sector back into AX

  mov   bx,BUFFER                         ; Buffer location into bx (needs to be at ES:BX)
  call  read_floppy_sectors               ; Read the sectors
  jc    .read_error                       ; Read error if carry flag set

  ; Search the root directory for stage3
  mov   cx,[BPB_ROOTENTCOUNT]             ; Number of root entries in CX
  mov   di,BUFFER                         ; Start at buffer pointer

.search:
  push  cx                                ; Stash counter
  mov   cx,11                             ; Names are 11 bytes
  mov   si,STAGE_3                        ; Comparing with our expected name...
  push  di                                ; rep will change DI
  rep cmpsb                               ; Compare strings...
  pop   di                                ; Restore DI
  pop   cx                                ; Restore counter...
  je    .load_stage3                      ; Match - jump to finding the starting cluster
  add   di,0x20                           ; ... advance DI 32-bytes to next filename...
  loop  .search                           ; ... and loop

  mov   si,NO_FILE                        ; If we're out of entries, load error message
  jmp   .show_error                       ; Show it, then die...

.load_stage3:
  mov   dx,[di + FAT_CLUSTER]             ; Get stage3 starting cluster

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

  mov   ebx,DATA_BUFFER                   ; And set BX up to load stage 3 sectors into DATA_BUFFER
  mov   edi,STAGE_3_LO_ADDR               ; And (32-bit) target buffer into EDI. Later this is mapped to the top 2GB

  ; TODO Fixed load position (currently at 0x100000)
  ;
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
  jc    .read_error                       ; Halt on error

  call  copy_data_buffer                  ; Copy data buffer to target location

  pop   dx                                ; Get current cluster back into DX (again 🙄)
  call  next_cluster                      ; Else, replace dx with next cluster...

  cmp   dx,0xFFF                          ; Is this the last cluster?
  je    .load_done                        ; Done if so...
  jmp   .load_loop                        ; ... and loop if not

.load_done:
  call  reset_floppy                      ; We're done with this from BIOS land, so reset it...  
  ret

.read_error:
  mov   si,READ_ERROR                     ; Load message...

.show_error:
  push  si
  mov   si,CRLF
  call  real_print_sz
  pop   si
  call  real_print_sz                     ; And print it

.die:
  hlt
  jmp .die


; Copy data buffer to target location
;
; Arguments:
;   EBX - Source pointer
;   EDI - target pointer
;   
; Modifies:
;   EDI - New target location
;
copy_data_buffer:
  push  ecx                               ; Save registers
  push  ebx
  push  eax

  xor   ecx,ecx                           ; Zero ECX for sector size calculation
  mov   ax,[BPB_BYTESPERSECT]             ; Bytes per sector in AX
  mov   cl,byte [BPB_SECTPERCLUST]        ; Sectors per cluster in CX
  imul  cx,ax                             ; Multiply them (we know we're on 386+ now 🎉)
  add   ecx,edi                           ; + buffer start to get ending position

;   mov   ecx,edi                           ; End at buffer...
;   add   ecx,0x200                         ; + 512 bytes

.loop
  mov   eax,dword [ebx]                   ; Get next dword
  mov   dword [ds:edi],eax                ; And copy it
  add   ebx,4                             ; Advance source pointer in ebx
  add   edi,4                             ; Advance dest pointer in edi
  cmp   edi,ecx                           ; Are we at the end?
  jne   .loop                             ; Loop if not

  pop   eax
  pop   ebx                               ; Else restore regs
  pop   ecx
  ret                                     ; And done
    

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
  push  cx; Restore regs

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

; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Data section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
READ_ERROR  db "Kernel read error; Halting", 0
NO_FILE     db "Kernel file not found; Halting", 0
CRLF        db 10, 13, 0
STAGE_3     db "KERNEL  BIN"
DOT         db '.'
bootDrv     db 0                            ; TODO This is hardcoded!!