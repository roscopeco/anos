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
; 
; What it does:
;
;   * Entry in real mode (from stage1)
;     * Prints a message as proof of life (with BIOS routines)
;     * Checks the CPU supports long mode (and dies with a message if not)
;     * Sets up the GDT with both 32- and 64-bit code / data segments
;   * Enables Unreal mode for a bit
;     * Loads stage3 at 0x00120000 using BIOS routines (still has to copy because using BIOS floppy)
;   * Enables 32-bit protected mode for a short time
;     * Sets up data and stack segments for protected mode
;     * Prints another message (direct to VGA mem, because, why not?)
;     * Checks if A20 is enabled, and tries to enable it (keyboard controller method only right now)
;     * Builds a basic page table to identity-map the first 2MB, so we can...
;   * Enable 64-bit long mode
;     * Does a bit more printing (I like printing, okay?)
;     * Initializes enough to run some simple C code
;     * Runs some simple C code (just a test for now)
;     * halts
;
; What it doesn't:
;
;   * Disable IRQ or NMI - works for now on the emulators, but will need it later
;   * Set up IDT - Didn't want to bother in 32-bit mode, I'll take the triple-faults...

bits 64
global _start

extern start_kernel
extern _bss_start, _bss_end               ; Linker defined symbols

section .text.init                        ; Linker needs to make sure this goes in first...

; Initialize C-land: Zero BSS, sort out memory pointer argument and call main
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

.done
  jmp   start_kernel                      ; Let's do some C...





