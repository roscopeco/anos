; stage3 - Kernel initializer for Limine boot protocol
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;
; Early initialization of environment and C runtime and calling out
; to limine-specific kernel entrypoint.
;
; From the C runtime perspective, this only does the bare minimum - 
; there's no global constructors or destructors, for example.
;
; .bss is zeroed, and the C entry point is called. That's it.
;
; Behind the scenes though, there's quite a bit that goes on to convert 
; from Limine's post-boot environment to the setup the kernel likes. 
;
; Briefly:
;
;   * We copy the kernel to the phys location we expect
;   * We set up our GDT and the TSSes the kernel expects
;   * We jump to C (limine_entrypoint.c) for most of the rest of the setup
;   * We do a bit of a two-step to set up our page tables
;   * We stash away some things Limine gave us
;     * Framebuffer (Limine exits boot services so we can't get this again)
;     * RSDP (Ditto above, and it's not lurking in low mem on UEFI systems)
;   * We do a bit of a trampoline bounce to switch CR3 and the stack
;   * Then we get rid of Limine's page tables
;     * _sort of_ - we're still not reclaiming bootloader memory yet...
;

bits 64
global _start_limine, bootstrap_trampoline

extern bsp_kernel_entrypoint_limine
extern _bss_start, _bss_end               ; Linker defined symbols
extern _kernel_vma_start, _kernel_vma_end ; .. ditto ...

%macro init_tss 1
  mov   rax,TSS%1
  mov   word [TSS%1_BASE_LOW],ax          ; Addresses here are already high,
  shr   rax,0x10                          ; in case you're wondering why I'm
  mov   byte [TSS%1_BASE_MID],al          ; not or'ing in the kernel base here...
  shr   rax,0x08
  mov   byte [TSS%1_BASE_HIGH],al
%endmacro

_start_limine:
  mov   rcx,_bss_end                      ; Get end of .bss section (VMA)
  mov   rax,_bss_start                    ; Get start of .bss section (VMA)
  sub   rcx,rax                           ; Compute length of .bss (bytes) in RCX
  shr   rcx,0x2                           ; Divide by 4 (we're zeroing dwords)

  test  rcx,rcx                           ; Do we have zero-size .bss?
  jz    .done                             ; We're done if so...

  mov   rbx,_bss_start                    ; bss start (VMA) into rbx
.zero_bss_loop:
  mov   dword [rbx],0x0                   ; Clear one dword
  add   rbx,0x4                           ; Increment write pointer
  dec   rcx                               ; Decrement loop counter
  jnz   .zero_bss_loop                    ; Loop until CX is zero

.init_gdt:
  lgdt  [GDT_DESC]                        ; We don't like Limine's GDT, let's use
                                          ; one that fits our needs...

  init_tss 0                              ; Init 16 TSS segments
  init_tss 1                              ; Note: this totally isn't scalable, it will change
  init_tss 2                              ; eventually to (probably) having the whole GDT in 
  init_tss 3                              ; managed memory rather than statically defined here...
  init_tss 4
  init_tss 5
  init_tss 6
  init_tss 7
  init_tss 8
  init_tss 9
  init_tss 10
  init_tss 11
  init_tss 12
  init_tss 13
  init_tss 14
  init_tss 15

  mov   ax,0x28                           ; Load the TSS (GDT selector 5 for the BSP)
  ltr   ax

.done:
  mov   ax,0x10                           ; Fix up the segments now we're using our GDT
  mov   ds,ax
  mov   es,ax
  mov   fs,ax
  mov   gs,ax
  mov   ss,ax

  mov   rax, cr0                          ; Enable SSE
  and   ax, 0xFFFB		                    ; ... clear coprocessor emulation CR0.EM
  or    ax, 0x2			                      ; ... set coprocessor monitoring  CR0.MP
  mov   cr0, rax

  mov   rax, cr4                          ; set CR4.OSFXSR and CR4.OSXMMEXCPT
  or    ax, 3 << 9
  mov   cr4, rax

  push  0x08                              ; Sort out code segment as well while we're at it...
  push  bsp_kernel_entrypoint_limine
  retfq                                   ; Rest of the init is in C...


; `limine_entrypoint.c` calls back to this to get the page tables and stack sorted 
; out without upsetting C too much.
;
; Arguments (mostly passed through):
;
;   rdi - system size
;   rsi - framebuffer width
;   rdx - framebuffer height
;   rcx - virtual address for new stack top
;   r8  - physical address of new PML4
;   r9  - address to bounce off to
;   
bootstrap_trampoline:                     ; (... except this bit, we'll pop back in a bit for this...)
  mov   cr3,r8
  mov   rsp,rcx                           ; (... because C doesn't appreciate having the stack changed...)

  push  0                                 ; Set up a fake stack frame for backtracing
  push  bootstrap_trampoline
  mov   rbp,0

  push  r9                                ; And let's get back to it!
  ret


align 16

%macro tss_segment 1
  dw 0x0067               ; 104 bytes for a TSS
TSS%1_BASE_LOW:
  dw 0                    ; Base (bits 0-15) - 0 (calculated at runtime)
TSS%1_BASE_MID:
  db 0                    ; Base (bits 16-23) - 0 (calculated at runtime)
  db 0b10001001           ; Access: 1 = Present, 00 = Ring 0, 0 = Type (system), 1001 = Long mode TSS (Available)
  db 0b00010000           ; Flags + Limit: 0 = byte granularity, 0 = 16-bit, 0 = Long mode, 1 = Available
TSS%1_BASE_HIGH:
  db 0                    ; Base (bits 23-31) - 0 (calculated at runtime)
  dd 0xFFFFFFFF           ; Base (bits 32-63) - 0xFFFFFFFF (in the identity-mapped kernel mem)
  dd 0                    ; Reserved
%endmacro

%macro define_tss 1
TSS%1:
  dd  0                   ; Reserved
  dq  0                   ; RSP0
  dq  0                   ; RSP1
  dq  0                   ; RSP2
  dq  0                   ; Reserved
  dq  0                   ; IST1
  dq  0                   ; IST2
  dq  0                   ; IST3
  dq  0                   ; IST4
  dq  0                   ; IST5
  dq  0                   ; IST6
  dq  df_stack+0x1000     ; IST7 - double fault
  dq  0                   ; Reserved
  dw  0                   ; Reserved
  dw  0                   ; IOPB
%endmacro

GDT:
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

  ; segments 5 to 20 - TSSn (one per supported CPU)
  ; Values within these are calculated in code...
  tss_segment 0
  tss_segment 1
  tss_segment 2
  tss_segment 3
  tss_segment 4
  tss_segment 5
  tss_segment 6
  tss_segment 7
  tss_segment 8
  tss_segment 9
  tss_segment 10
  tss_segment 11
  tss_segment 12
  tss_segment 13
  tss_segment 14
  tss_segment 15

align 16

GDT_DESC:
  ; GDT Descriptor
  dw  GDT_DESC-GDT-1      ; Size (computed from here - start)
  dq  GDT                 ; Address (GDT, above)

; Define the actual TSSes - but don't set RSP0 up yet,
; we'll sort those out in the kernel when we're actually
; task switching...
;
align 16
define_tss 0
align 16
define_tss 1
align 16
define_tss 2
align 16
define_tss 3
align 16
define_tss 4
align 16
define_tss 5
align 16
define_tss 6
align 16
define_tss 7
align 16
define_tss 8
align 16
define_tss 9
align 16
define_tss 10
align 16
define_tss 11
align 16
define_tss 12
align 16
define_tss 13
align 16
define_tss 14
align 16
define_tss 15

df_stack:
  resb 0x1000

