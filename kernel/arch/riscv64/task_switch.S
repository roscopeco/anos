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

# task_do_switch(Task *next)
# a0 = next task
task_do_switch:
    # Save current task state if it exists
    # tp register points to per-CPU data (similar to GS in x86)
    ld t2, CPU_TASK_CURRENT(tp)
    beqz t2, .restore_next # If no current task, skip saving

    # Save general purpose registers
    # We need to save: ra, sp, gp, t0-t6, s0-s11
    # Note: a2-a7 are caller-saved, so we don't need to save them
    #       (a0-a1 are too, but we use them for passing args to the bootstraps so we do need them).
    #
    addi sp, sp, -448    # Allocate space for registers - (MUST EQUAL TASK_SAVED_REGISTER_BYTES + 16)
    sd   a0, 0*8(sp)     # where the +16 is for RA and SEPC that we don't count as saved registers from the task POV.
    sd   a1, 1*8(sp)
    sd   gp, 2*8(sp)
    sd   t0, 3*8(sp)
    sd   t1, 4*8(sp)
    sd   t2, 5*8(sp)
    sd   t3, 6*8(sp)
    sd   t4, 7*8(sp)
    sd   t5, 8*8(sp)
    sd   t6, 9*8(sp)
    sd   s0, 10*8(sp)
    sd   s1, 11*8(sp)
    sd   s2, 12*8(sp)
    sd   s3, 13*8(sp)
    sd   s4, 14*8(sp)
    sd   s5, 15*8(sp)
    sd   s6, 16*8(sp)
    sd   s7, 17*8(sp)
    sd   s8, 18*8(sp)
    sd   s9, 19*8(sp)
    sd   s10, 20*8(sp)
    sd   s11, 21*8(sp)

    # Save floating point state if enabled
    csrr t0, sstatus
    slli t1, t0, 51        # shift left so bits 13:12 are now at 63:62
    srli t1, t1, 63        # shift back to bottom -> if FS != 0, t1 = 1
    beqz t1, .skip_fp_save
    fsd  f0, 22*8(sp)
    fsd  f1, 23*8(sp)
    fsd  f2, 24*8(sp)
    fsd  f3, 25*8(sp)
    fsd  f4, 26*8(sp)
    fsd  f5, 27*8(sp)
    fsd  f6, 28*8(sp)
    fsd  f7, 29*8(sp)
    fsd  f8, 30*8(sp)
    fsd  f9, 31*8(sp)
    fsd  f10, 32*8(sp)
    fsd  f11, 33*8(sp)
    fsd  f12, 34*8(sp)
    fsd  f13, 35*8(sp)
    fsd  f14, 36*8(sp)
    fsd  f15, 37*8(sp)
    fsd  f16, 38*8(sp)
    fsd  f17, 39*8(sp)
    fsd  f18, 40*8(sp)
    fsd  f19, 41*8(sp)
    fsd  f20, 42*8(sp)
    fsd  f21, 43*8(sp)
    fsd  f22, 44*8(sp)
    fsd  f23, 45*8(sp)
    fsd  f24, 46*8(sp)
    fsd  f25, 47*8(sp)
    fsd  f26, 48*8(sp)
    fsd  f27, 49*8(sp)
    fsd  f28, 50*8(sp)
    fsd  f29, 51*8(sp)
    fsd  f30, 52*8(sp)
    fsd  f31, 53*8(sp)
.skip_fp_save:
    sd   ra, 54*8(sp)   # NOTE: RA is **expected** to go exactly here on the stack,
                        #     : and everything is setup in task_create_new for that.
                        #     : it is **not** counted in the SAVED_REGISTER_COUNT

    # Store current stack pointers
    csrr t0, sscratch
    sd t0, TASK_RSP0(t2)
    sd sp, TASK_SSP(t2)

    csrr t0, sepc
    sd   t0, 55*8(sp)

.restore_next:
    # Load next task
    sd a0, CPU_TASK_CURRENT(tp)

    # Restore next task's stack
    ld t0, TASK_RSP0(a0)
    csrw sscratch, t0
    ld sp, TASK_SSP(a0)

    # Switch to next task's pagetables
    ld t0, TASK_PML4(a0)
    csrw satp, t0
    sfence.vma

    ld  t0, 55*8(sp)
    csrw sepc, t0

    # Restore floating point state if enabled
    csrr t0, sstatus
    slli t1, t0, 51
    srli t1, t1, 63
    beqz t1, .skip_fp_restore
    fld  f0, 22*8(sp)
    fld  f1, 23*8(sp)
    fld  f2, 24*8(sp)
    fld  f3, 25*8(sp)
    fld  f4, 26*8(sp)
    fld  f5, 27*8(sp)
    fld  f6, 28*8(sp)
    fld  f7, 29*8(sp)
    fld  f8, 30*8(sp)
    fld  f9, 31*8(sp)
    fld  f10, 32*8(sp)
    fld  f11, 33*8(sp)
    fld  f12, 34*8(sp)
    fld  f13, 35*8(sp)
    fld  f14, 36*8(sp)
    fld  f15, 37*8(sp)
    fld  f16, 38*8(sp)
    fld  f17, 39*8(sp)
    fld  f18, 40*8(sp)
    fld  f19, 41*8(sp)
    fld  f20, 42*8(sp)
    fld  f21, 43*8(sp)
    fld  f22, 44*8(sp)
    fld  f23, 45*8(sp)
    fld  f24, 46*8(sp)
    fld  f25, 47*8(sp)
    fld  f26, 48*8(sp)
    fld  f27, 49*8(sp)
    fld  f28, 50*8(sp)
    fld  f29, 51*8(sp)
    fld  f30, 52*8(sp)
    fld  f31, 53*8(sp)
.skip_fp_restore:

    # Restore general purpose registers
    ld a0, 0*8(sp)
    ld a1, 1*8(sp)
    ld gp, 2*8(sp)
    ld t0, 3*8(sp)
    ld t1, 4*8(sp)
    ld t2, 5*8(sp)
    ld t3, 6*8(sp)
    ld t4, 7*8(sp)
    ld t5, 8*8(sp)
    ld t6, 9*8(sp)
    ld s0, 10*8(sp)
    ld s1, 11*8(sp)
    ld s2, 12*8(sp)
    ld s3, 13*8(sp)
    ld s4, 14*8(sp)
    ld s5, 15*8(sp)
    ld s6, 16*8(sp)
    ld s7, 17*8(sp)
    ld s8, 18*8(sp)
    ld s9, 19*8(sp)
    ld s10, 20*8(sp)
    ld s11, 21*8(sp)
    ld ra, 54*8(sp)
    addi sp, sp, 448     # Restore stack pointer - (MUST EQUAL TASK_SAVED_REGISTER_BYTES + 16)

    # Return to the next task
    ret 