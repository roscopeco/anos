# stage3 - Base non-circular singly-linked node list
# anos - An Operating System
#
# Copyright (c) 2023 Ross Bamford
#
# Implemented in asm for efficient calling from either C or assembly
#
# RISC-V 64-bit implementation
#

.option norvc
.section .text
.globl list_add
.globl list_insert_after
.globl list_delete_after
.globl list_find

.equ NODE_NEXT, 0x00

# Insert after func# C-callable (RISC-V LP64 ABI)
#
# Args:
# a0 target node to insert after
# a1 subject node to insert
#
# Returns:
# a0 the subject node
#
list_insert_after:
    beqz    a0, .link_subject            # Is target null? Just link subject if so
    ld      t0, NODE_NEXT(a0)            # otherwise, grab the current next...
    sd      a1, NODE_NEXT(a0)            # and set it to the subject instead
    j       .check_subject

.link_subject:
    li      t0, 0                         # target was null, so next is null

.check_subject:
    beqz    a1, .done                     # Is the subject null? Skip setting its pointer if so
    sd      t0, NODE_NEXT(a1)             # link subject to the saved next (or zero if target was null)

.done:
    mv      a0, a1                        # Return the subject
    ret


# Add-at-end func# C-callable (RISC-V LP64 ABI)
#
# Args:
# a0 list head
# a1 subject node to insert
#
# Returns:
# a0 the subject node
#
list_add:
    beqz    a0, .found_end                # Is target null? If so, we've found the end

.loop:
    ld      t0, NODE_NEXT(a0)             # Get next node
    beqz    t0, .found_end                # Is it null? If so, a0 contains the end of the list
    
    mv      a0, t0                        # Else, move to next node
    j       .loop
    
.found_end:
    j       list_insert_after             # Now we have the end, it's just an insert_after...


# Delete after func# C-callable (RISC-V LP64 ABI)
#
# Args:
# a0 target node to delete the following node from
#
# Returns:
# a0 the deleted node
#
list_delete_after:
    beqz    a0, .null_target              # Is target null?

    ld      t0, NODE_NEXT(a0)             # Get next node (the one to delete) into t0
    beqz    t0, .delete_done              # Is it null? If so, it's already "deleted"

    ld      t1, NODE_NEXT(t0)             # Else, get its next node into t1
    sd      t1, NODE_NEXT(a0)             # Set target's next to the deleted node's next
    sd      zero, NODE_NEXT(t0)           # And clear next in the deleted node
    mv      a0, t0                        # Return the deleted node
    ret

.null_target:
    li      a0, 0                         # Return null
    ret

.delete_done:
    ret


# Find func# C-callable (RISC-V LP64 ABI)
#
# The predicate function should take a single
# argument (a list node pointer) in a0 and return
# a C `bool` (a uint8_t) in a0,
# 1 for match, 0 otherwise.
#
# The argument to the predicate function is
# guaranteed to never be null.
#
# Args:
# a0 list head
# a1 predicate function pointer
#
# Returns:
# a0 the matching node, or null
#
list_find:
    addi    sp, sp, -16                  # Adjust stack to save return address and a1
    sd      ra, 8(sp)                    # Save return address
    sd      a1, 0(sp)                    # Save predicate function

    beqz    a0, .find_null               # Is head null? Done if so
    beqz    a1, .find_null               # Is predicate null? Done if so

.find_loop:
    sd      a0, 0(sp)                    # Save current node
    jalr    a1                           # Call the predicate (arg already in a0)
    ld      t0, 0(sp)                    # Restore current node to t0
    bnez    a0, .found_match             # Did predicate return nonzero? If so, we found a match

    ld      a0, NODE_NEXT(t0)            # Else, next node into a0
    beqz    a0, .find_null               # Is it null? Exit loop if so
    
    ld      a1, 0(sp)                    # Restore predicate function pointer
    j       .find_loop                   # Continue loop

.found_match:
    mv      a0, t0                       # Return matching node

.find_done:
    ld      ra, 8(sp)                    # Restore return address
    addi    sp, sp, 16                   # Restore stack
    ret

.find_null:
    li      a0, 0                        # Return null
    j       .find_done 