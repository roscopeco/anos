; stage3 - Kernel initializer
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Initial (super basic) initialization of C runtime and calling out
; to kernel startup.
;
; This only does the bare minimum - there's no global constructors
; or destructors, for example.
;
; .bss is zeroed, and the C entry poit is called. That's it.
;
; What it expects:
;
;   * Entry point at 0x00120000 in 64-bit long mode (not compatibility)
;   * Stack already set up somewhere (we'll move it in a bit)
;   * Interrupts disabled
;   * VGA 80x26 16-color text mode
;   * RDI contains address of memory map from stage2
; 
; What it does:
;
;   * Zeroes BSS
;   * Fixes the GDTR and TR to point to linear addresses (Might move this to stage2)
;   * Calls out to the C entry point
;

bits 64
global _start

extern bsp_kernel_entrypoint_bios
extern _bss_start, _bss_end               ; Linker defined symbols

section .text.init                        ; Linker needs to make sure this goes in first...

; Initialize C-land: Zero BSS, sort out memory pointer argument and call main
; This also moves the GDT into the top 2GB virtual address, so things don't break 
; when I get rid of the bottom 2MiB identity map during kernel bootstrap (or,
; more correctly, the first time there's an exception after that).
;
_start:
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

  sgdt  [.GDT_DESC]                       ; Get the current GDTR
  mov   rax,[.GDT_DESC+2]                 ; Move the address into RAX
  or    rax,0xFFFFFFFF80000000            ; Or with kernel-space base address
  mov   [.GDT_DESC+2],rax                 ; Put back into the structure
  lgdt  [.GDT_DESC]                       ; And load back to GDTR.
                                          ; No need to reload CS or anything here, it's
                                          ; fine until next time there's an exception...

.done:
  mov   rax, cr0                          ; Enable SSE
  and   ax, 0xFFFB		                    ; ... clear coprocessor emulation CR0.EM
  or    ax, 0x2			                      ; ... set coprocessor monitoring  CR0.MP
  mov   cr0, rax

  mov   rax, cr4                          ; set CR4.OSFXSR and CR4.OSXMMEXCPTqq
  or    ax, 3 << 9
  mov   cr4, rax

  jmp   bsp_kernel_entrypoint_bios        ; Let's do some C...

.GDT_DESC:
  ; GDT Descriptor
  dw  0      ; Size (computed from here - start)
  dq  0                 ; Address (GDT, above)




