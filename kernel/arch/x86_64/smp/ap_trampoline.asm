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
; The basic idea here is to get the AP from real mode to long with as
; little fuss as possible, then set up the bare-minimum of what needs
; setting up before we jump into the kernel code proper. 
;
; The startup code on the BSP has already set up stacks for each possible
; AP, and there is already a qword waiting, which is the address the
; AP should "return to" once it's finished setting things up.
;
; The AP will atomically grab the next available number from the `ap_count`
; and then select its stack at a suitable offset from 0x9000 based on that
; number. This unique id is passed to the main kernel AP startup code
; as the first parameter.
;
; The `ap_flag` is holding the BSP, which will wait 10ms for the AP to
; set the flag before it's marked as ignored. Currently this doesn't 
; mean much - it's possible for a "slow" AP to continue the boot and end
; up in the scheduler, but nothing will ever get scheduled on it because
; the kernel thinks it's failed so it'll just run its idle thread.
;
; Bear in mind the `ap_flag` is shared between APs, so we can (currently)
; only boot one at a time, which probably doesn't much matter...
;
; The job really then boils down to:
;
;   * Get to protected mode
;   * Set up paging & go to long mode
;   * Get unique ID
;   * Set up stack
;   * Set flag to unlock the BSP
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

  lgdt  [AP_GDT_DESC]                     ; Load GDT reg with the (temporary) descriptor
                                          ; We'll replace this with the kernel's full 64 bit
                                          ; GDT once we get to long mode...

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

  jmp   0x08:main64                       ; And go long!

bits 64
main64:
  lgdt  [k_gdtr]                          ; Load the fully 64 bit kernel GDT we were given.

  mov ax,0x10                             ; Set ax to 0x10 - offset of segment 2, the kernel's 64-bit data segment...
  mov ds,ax                               ; and set DS to that..
  mov es,ax                               ; and ES too...
  mov ss,ax                               ; as well as SS.

  mov rdi, qword [ap_count]               ; There's a counter in the (shared) BSS,
  
.get_id:                                  ; from which each AP gets a unique ID...
  mov rbx, rdi                            ; by just atomically grabbing the next
  inc rbx                                 ; number and using that...
  lock cmpxchg qword [ap_count], rbx      ; NOTE that it doesn't necessarily match
  jnz .get_id                             ; LAPIC or CPU IDs!

  mov rax,rdi                             ; Set up a stack for protected mode based on unique id...
  shl rax,STACK_SHIFT                     ; ... which we'll shift left by 4
  or  rax,STACKS_BASE                     ; ... and OR in the long-mode stack base
  add rax,STACK_START_OFS                 ; ... then point to stack top, minus the return address already stacked...
  mov rsp,rax                             ; ... and we're good!

  mov rcx,rdi                             ; Set up the TSS for this specific core,
  shl rcx,4                               ; based on the unique ID we grabbed at
  add rcx,0x28                            ; the beginning. NOTE: This may differ
  ltr cx                                  ; from the hardware CPU / LAPIC ID for this CPU!

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

  ; We don't need a TSS in this GDT, we set tr up after switching to the kernel GDT...

AP_GDT_DESC:
  ; GDT Descriptor
  dw  AP_GDT_DESC-AP_GDT-1; Size (computed from here - start)
  dd  AP_GDT              ; Address (GDT, above)


section .bss

ap_count  resq  1         ; Unique ID flag
k_pml4    resq  1         ; Kernel PML4 (physical)
ap_flag   resq  1         ; AP booted flag
k_gdtr    resd  3         ; Kernel GDT
reserved2 resd  1
k_idtr    resd  3         ; Kernel IDT
reserved3 resd  1
