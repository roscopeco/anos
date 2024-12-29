; stage3 - Base non-circular singly-linked node list
; anos - An Operating System
;
; Copyright (c) 2023 Ross Bamford
;
; Implemented in asm for efficient calling from either C or assembly
;

%ifdef asm_macho64
%define FUNC(name) _ %+ name
%else
%define FUNC(name) name
%endif

bits 64
global FUNC(list_add), FUNC(list_insert_after), FUNC(list_delete_after), FUNC(list_find)

%define NODE_NEXT 0x00 
%define NODE_SIZE 0x08
%define NODE_TYPE 0x10

; Insert after func; C-callable (SYSV AMD64)
;
; Args:
; rdi target node to insert after
; rsi subject node to insert
;
; Modifies:
; rax return (the subject node)
; rdx trashed
;
FUNC(list_insert_after):
  xor   rdx,rdx                           ; Zero out rdx
  test  rdi,rdi                           ; Is target null?
  jz   .link_subject                      ; Just link subject if so

  mov   rdx,[rdi+NODE_NEXT]               ; otherwise, grab the current next...
  mov   [rdi+NODE_NEXT],rsi               ; and set it to the subject instead

.link_subject:
  test  rsi,rsi                           ; Is the subject null?
  jz    .done                             ; Skip setting its pointer if so

  mov   [rsi+NODE_NEXT],rdx               ; to the saved next (or zero if target was null).

.done:
  mov   rax,rsi                           ; And return the subject
  ret 


; Add-at-end func; C-callable (SYSV AMD64)
;
; Args:
;   rdi list head
;   rsi subject node to insert
;
; Modifies:
;   rax return (the subject node)
;   rdx trashed
;
FUNC(list_add):
  test  rdi,rdi                           ; Is target null?
  jz    .found_end                        ; If so, we've already found the end of the list 🥳
  
.loop:
  mov   rax,[rdi+NODE_NEXT]               ; Get next node
  test  rax,rax                           ; Is it null?
  jz    .found_end                        ; If so, rdi contains the end of the list
  
  mov   rdi,rax                           ; Else, loop again
  jmp   .loop
  
.found_end:
  jmp   FUNC(list_insert_after)           ; Now we have the end, it's just an insert_after...


; Delete after func; C-callable (SYSV AMD64)
;
; Args:
;   rdi target node to delete the following node from
;
; Modifies:
;   rax return (the deleted node)
;   rdx trashed
;
FUNC(list_delete_after):
  test  rdi,rdi                           ; Is target null?
  jnz   .nonnull                          ; Continue if not

  xor   rax,rax                           ; Else, nothing more to do!
  ret   

.nonnull:
  mov   rax,[rdi+NODE_NEXT]               ; Get next node (the one to delete) into rax
  test  rax,rax                           ; Is it null?
  jz    .done                             ; If so, it's already "deleted"

  mov   rdx,[rax+NODE_NEXT]               ; Else, get _its_ next node into rdx
  mov   [rdi+NODE_NEXT],rdx               ; set target's next to the deleted node's next
  mov   qword [rax+NODE_NEXT], 0          ; And clear next in the deleted node
  ret

.done:
  ret 
  

; Find func; C-callable (SYSV AMD64)
;
; The predicate function should take a single
; argument (a list node pointer) in rdi and return
; a C `bool` (a uint8_t) in rax (actually, al),
; 1 for match, 0 otherwise.
;
; The argument to the predicate function is
; guaranteed to never be null.
;
; Args:
;   rdi list head
;   rsi predicate function pointer
;
; Modifies:
;   rax return (the matching node, or null)
;
FUNC(list_find):
  push  rbp                               ; Might call back to C, so maintain alignment...
  mov   rbp,rsp                           ; ... by making a stack frame (for the push really).

  test  rdi,rdi                           ; Is head null?
  jz    .isnull                           ; Done if so

  test  rsi,rsi                           ; Is predicate null?
  jnz   .nonnull                          ; Continue if not

.isnull:
  xor   rax,rax                           ; nothing to find
  leave                                   ; get rid of stack frame (using base pointer)
  ret
  
.nonnull:
  push  rdi                               ; Save regs for potential C callback
  push  rsi
  call  rsi                               ; Call the predicate (arg already in rdi)
  pop   rsi                               ; Restore the regs
  pop   rdi

  test  al,al                             ; Did predicate return nonzero?
  jnz   .done                             ; Done if so - rdi contains matching node
  
  mov   rdi,[rdi+NODE_NEXT]               ; Else, next node into rdi
  
  test  rdi,rdi                           ; Is it null?
  jnz   .nonnull                          ; loop if not.
  
.done: 
  mov rax,rdi                             ; Node (or null) into rax, and done.
  leave                                   ; get rid of stack frame (using base pointer)
  ret

