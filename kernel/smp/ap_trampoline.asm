bits 16

section .text.init
global _start
extern STACKS_BASE, STACK_SHIFT, STACK_START_OFS

; This is the AP trampoline.
;
; This code is at 0x0100:0000 (0x1000 linear), and we enter in real mode.
; BSS is at 0x1000, and each AP has its own 2KiB stack in the region starting
; from 0x6000. The real-mode code is responsible for setting that up 
; (keep reading).
;
; The BSS will have a few things already set up by the SMP startup code:
;
;   * ap_count  - A counter tracking starting APs (starts at 1)
;   * k_pml4    - The physical address of the kernel page-tables
;   * ap_flag   - A flag the AP should set if it makes it to long mode
;
; In real mode, the AP will atomically grab the next available number
; from he `ap_count`, and stash it away. It'll then set up its 16-byte
; stack at a suitable offset from 0x9000 based on that number.
;
; On the stack there is already a qword waiting, which is the address the
; AP should "return to" once it's finished setting things up.
;
; The `ap_flag` is holding the BSP, currently indefinitely, but there
; will probably need to be a timeout or something in case of a faulty
; AP - but either way, the AP needs to set it to unblock the BSP.
;
; Bear in mind the `ap_flag` is shared between APs, so we can (currently)
; only boot one at a time, which probably doesn't much matter...
;
; The job really then boils down to:
;
;   * Get unique ID
;   * Set up stack
;   * Go to protected mode
;   * Set up paging & go to long mode
;   * Set flag to unlock the BSP
;   * Put unique ID in RDI
;   * Clear regs, housekeeping, then "return"
;
; Once those things are done, the regular kernel code takes over
; (see `ap_kernel_entrypoint` in `smp/startup.c`).
;
; NOTE: Probably also worth noting that this is linked weirdly;
; see notes in `realmode.ld` if you're interested in that.
;
_start:
  cli
  cld

.get_id:                                  ; Each AP gets a unique ID...
  mov ax, WORD [ap_count]                 ; There's a counter in the (shared) BSS,
  mov bx, ax                              ; each AP just atomically grabs the next
  inc bx                                  ; number and uses that...
  lock cmpxchg WORD [ap_count], bx        ; NOTE that it doesn't necessarily match
  jnz .get_id                             ; LAPIC or CPU IDs!

  mov cx, ax                              ; Stash the unique ID in cx for now...

  lgdt  [AP_GDT_DESC]                     ; Load GDT reg with the (temporary) descriptor

  ; Jump to protected mode
  mov   eax,cr0                           ; Get control register 0 into eax
  or    al,1                              ; Set PR bit (enable protected mode)
  mov   cr0,eax                           ; And put back into cr0

  jmp   0x18:main32                       ; Far jump to main32, use GDT selector at offset 0x18 (32-bit code).

bits 32
main32:
  mov   ax,0x20                           ; Set ax to 0x20 - offset of segment 4, the 32-bit data segment...
  mov   ds,ax                             ; and set DS to that..
  mov   es,ax                             ; and ES too...
  mov   ss,ax                             ; as well as SS.

  xor   eax,eax                           ; Set up a stack for protected mode
  mov   ax,cx                             ; CX still has this AP's unique ID
  shl   eax,STACK_SHIFT                   ; ... which we'll shift left by 4
  or    eax,STACKS_BASE                   ; ... and OR in the stack base
  add   eax,STACK_START_OFS               ; ... then add 8 to point to stack top, minus the return address already stacked...
  mov   esp,eax                           ; ... and we're good!

  push  0                                 ; Stack the BSP unique id
  push  ecx                               ; ... as 64-bit


;;; GO LONG

  mov   eax,0b10100000                    ; Set PAE and PGE bits
  mov   cr4,eax

  mov   eax,[k_pml4]                      ; Load PML4 into cr3
  mov   cr3,eax

  mov   ecx,0xC0000080                    ; Read EFER MSR
  rdmsr

  or    eax,0x100                         ; Set the long-mode enable bit
  wrmsr                                   ; and write back to EFER MSR

  mov   eax,cr0                           ; Activate paging to go full long mode
  or    eax,0x80000000                    ; (we're already in protected mode)
  mov   cr0,eax

  jmp   0x08:main64                       ; Go go go


; ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 64-bit section
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
bits 64
main64:
  or  rsp,0xffffffff80000000              ; Fix up stack for long mode

  pop rdi                                 ; Pop the unique ID we pushed earlier,
                                          ; and set it up as argument to the 
                                          ; entrypoint we're about to "return" to...

  lgdt  [k_gdtr]                          ; Load the kernel GDT we were given

  mov rcx,rdi                             ; Set up the TSS for this specific core,
  shl rcx,4                               ; based on the unique ID we grabbed at
  add rcx,0x28                            ; the beginning. NOTE: This may differ
  ltr cx                                  ; from the CPU / LAPIC ID for this CPU!

  lidt  [k_idtr]                          ; Load the kernel IDT we were given

  mov qword [ap_flag], 1                  ; We made it to long mode, let the bsp know

  xor rax, rax                            ; Zero out the rest of the GP registers...
  xor rbx, rbx
  xor rcx, rcx
  xor rdx, rdx
  xor rsi, rsi
  xor rbp, rbp
  xor r8,  r8
  xor r9,  r9
  xor r10, r10
  xor r11, r11
  xor r12, r12
  xor r13, r13
  xor r14, r14
  xor r15, r15

  ret                                     ; And "return" to the AP entrypoint

align 16

; TODO This is copied pretty-much verbatim from Stage 2!
;

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


section .bss

ap_count  resw  1         ; Unique ID flag
reserved  resb  6
k_pml4    resq  1         ; Kernel PML4 (physical)
ap_flag   resq  1         ; AP booted flag
k_gdtr    resd  3         ; Kernel GDT
reserved2 resd  1
k_idtr    resd  3         ; Kernel IDT
reserved3 resd  1
