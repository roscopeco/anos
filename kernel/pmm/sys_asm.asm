
;
; stage3 - Physical memory subsystem routines
; anos - An Operating System
;
; Copyright (c) 2025 Ross Bamford
;

global get_pagetable_root, set_pagetable_root

get_pagetable_root:
    mov rax,cr3
    ret

set_pagetable_root:
    mov cr3,rax
    ret
