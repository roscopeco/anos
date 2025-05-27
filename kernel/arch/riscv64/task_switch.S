# stage3 - RISC-V task switch handler (S-mode)
# anos - An Operating System
#
# Copyright (c) 2024 Ross Bamford

.section .text
.global task_do_switch

# Task struct offsets (must match task.h)
.equ TASK_DATA, 8
.equ TASK_RSP0, 24
.equ TASK_SSP, 32
.equ TASK_PML4, 48

# CPU state offsets (must match state.h)
.equ CPU_TASK_CURRENT, 928
.equ CPU_TASK_TSS, 936

# task_do_switch(Task *next)
# a0 = next task
task_do_switch:
    # Save current task state if it exists
    # tp register points to per-CPU data (similar to GS in x86)
    lw t2, CPU_TASK_CURRENT(tp)
    beqz t2, .restore_next # If no current task, skip saving

    # Save general purpose registers
    # We need to save: ra, sp, gp, t0-t6, s0-s11
    # Note: a0-a7 are caller-saved, so we don't need to save them
    # Note: tp is per-CPU data pointer, we don't save/restore it
    addi sp, sp, -256    # Allocate space for registers
    sw ra, 0*8(sp)
    sw sp, 1*8(sp)
    sw gp, 2*8(sp)
    sw t0, 3*8(sp)
    sw t1, 4*8(sp)
    sw t2, 5*8(sp)
    sw t3, 6*8(sp)
    sw t4, 7*8(sp)
    sw t5, 8*8(sp)
    sw t6, 9*8(sp)
    sw s0, 10*8(sp)
    sw s1, 11*8(sp)
    sw s2, 12*8(sp)
    sw s3, 13*8(sp)
    sw s4, 14*8(sp)
    sw s5, 15*8(sp)
    sw s6, 16*8(sp)
    sw s7, 17*8(sp)
    sw s8, 18*8(sp)
    sw s9, 19*8(sp)
    sw s10, 20*8(sp)
    sw s11, 21*8(sp)

    # Save floating point state if enabled
    csrr t0, sstatus
    slli t1, t0, 51        # shift left so bits 13:12 are now at 63:62
    srli t1, t1, 63        # shift back to bottom -> if FS != 0, t1 = 1
    beqz t1, .skip_fp_save
    fsd f0, 22*8(sp)
    fsd f1, 23*8(sp)
    # ... save all FP registers ...
    fsd f31, 53*8(sp)
.skip_fp_save:

    # Store current stack pointer
    sw sp, TASK_SSP(t2)

.restore_next:
    # Load next task
    sw a0, CPU_TASK_CURRENT(tp)

    # Restore next task's stack
    lw sp, TASK_SSP(a0)

    # Restore floating point state if enabled
    csrr t0, sstatus
    slli t1, t0, 51
    srli t1, t1, 63
    beqz t1, .skip_fp_restore
    fld f0, 22*8(sp)
    fld f1, 23*8(sp)
    # ... restore all FP registers ...
    fld f31, 53*8(sp)
.skip_fp_restore:

    # Restore general purpose registers
    lw ra, 0*8(sp)
    lw gp, 2*8(sp)
    lw t0, 3*8(sp)
    lw t1, 4*8(sp)
    lw t2, 5*8(sp)
    lw t3, 6*8(sp)
    lw t4, 7*8(sp)
    lw t5, 8*8(sp)
    lw t6, 9*8(sp)
    lw s0, 10*8(sp)
    lw s1, 11*8(sp)
    lw s2, 12*8(sp)
    lw s3, 13*8(sp)
    lw s4, 14*8(sp)
    lw s5, 15*8(sp)
    lw s6, 16*8(sp)
    lw s7, 17*8(sp)
    lw s8, 18*8(sp)
    lw s9, 19*8(sp)
    lw s10, 20*8(sp)
    lw s11, 21*8(sp)
    addi sp, sp, 256     # Restore stack pointer

    # Return to the next task
    ret 