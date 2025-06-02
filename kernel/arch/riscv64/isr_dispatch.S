# stage3 - RISC-V trap dispatcher (S-mode)
# anos - An Operating System
#
# Copyright (c) 2025 Ross Bamford

    .option norvc
    .section .text
    .global supervisor_trap_dispatcher
    .align 16

# Task struct offsets (must match task.h)
.equ TASK_DATA, 8
.equ TASK_RSP0, 24
.equ TASK_SSP, 32
.equ TASK_PML4, 48

# CPU state offsets (must match state.h)
.equ CPU_TASK_CURRENT, 928
.equ CPU_TASK_TSS, 936

.macro _trap_dispatcher_impl idx
    addi    sp, sp, -320
    sd      tp, 304(sp)
    sd      ra, 296(sp)
    sd      t0, 288(sp)
    frcsr   t0
    sw      t0, 280(sp)
    sd      t1, 272(sp)
    sd      t2, 264(sp)
    sd      a0, 256(sp)
    sd      a1, 248(sp)
    sd      a2, 240(sp)
    sd      a3, 232(sp)
    sd      a4, 224(sp)
    sd      a5, 216(sp)
    sd      a6, 208(sp)
    sd      a7, 200(sp)
    sd      t3, 192(sp)
    sd      t4, 184(sp)
    sd      t5, 176(sp)
    sd      t6, 168(sp)
    fsd     ft0, 152(sp)
    fsd     ft1, 144(sp)
    fsd     ft2, 136(sp)
    fsd     ft3, 128(sp)
    fsd     ft4, 120(sp)
    fsd     ft5, 112(sp)
    fsd     ft6, 104(sp)
    fsd     ft7, 96(sp)
    fsd     fa0, 88(sp)
    fsd     fa1, 80(sp)
    fsd     fa2, 72(sp)
    fsd     fa3, 64(sp)
    fsd     fa4, 56(sp)
    fsd     fa5, 48(sp)
    fsd     fa6, 40(sp)
    fsd     fa7, 32(sp)
    fsd     ft8, 24(sp)
    fsd     ft9, 16(sp)
    fsd     ft10, 8(sp)
    fsd     ft11, 0(sp)

    # TODO handling syscalls here is overdoing it a bit, there's no need
    #      to do all the saving and restoring for those...
    #
    #      Changing it would mean switching to vectored rather than direct
    #      traps, so for now this is good enough.

    csrr    t0, scause          # Load trap cause
    li      t1, 8               # U-mode ecall
    beq     t0, t1, handle_user_ecall_\idx
    li      t1, 12              # Instruction page fault
    beq     t0, t1, handle_page_fault_exec_\idx
    li      t1, 13              # Load page fault
    beq     t0, t1, handle_page_fault_read_\idx
    li      t1, 15              # Store/AMO page fault
    beq     t0, t1, handle_page_fault_write_\idx

    # Unknown trap - panic
    csrr    a1, sepc
    csrr    a0, scause

    tail    unknown_trap_handler

handle_user_ecall_\idx:
    mv      a5, a7
    csrr    a7, sepc
    addi    a7, a7, 4
    csrw    sepc, a7
    call    handle_syscall_69
    j       handled_no_a0_\idx

handle_page_fault_exec_\idx:
    li      a0, 8           # a0 = code (eight for insn fetch, PG_EXEC is set)
    j       handle_page_fault_\idx

handle_page_fault_read_\idx:
    li      a0, 2           # a0 = code (two for read, PG_READ is set)
    j       handle_page_fault_\idx

handle_page_fault_write_\idx:
    li      a0, 4           # a0 = code (four for write, PG_WRITE is set)

handle_page_fault_\idx:
    csrr    a1, stval       # a1 = fault address
    csrr    a2, sepc        # a2 = origin address

    call    page_fault_wrapper

handled_\idx:
    ld      a0, 256(sp)
handled_no_a0_\idx:
    ld      tp, 304(sp)
    ld      ra, 296(sp)
    lw      t0, 280(sp)
    fscsr   t0
    ld      t0, 288(sp)
    ld      t1, 272(sp)
    ld      t2, 264(sp)
    ld      a1, 248(sp)
    ld      a2, 240(sp)
    ld      a3, 232(sp)
    ld      a4, 224(sp)
    ld      a5, 216(sp)
    ld      a6, 208(sp)
    ld      a7, 200(sp)
    ld      t3, 192(sp)
    ld      t4, 184(sp)
    ld      t5, 176(sp)
    ld      t6, 168(sp)
    fld     ft0 ,152(sp)
    fld     ft1, 144(sp)
    fld     ft2, 136(sp)
    fld     ft3, 128(sp)
    fld     ft4, 120(sp)
    fld     ft5, 112(sp)
    fld     ft6, 104(sp)
    fld     ft7, 96(sp)
    fld     fa0, 88(sp)
    fld     fa1, 80(sp)
    fld     fa2, 72(sp)
    fld     fa3, 64(sp)
    fld     fa4, 56(sp)
    fld     fa5, 48(sp)
    fld     fa6, 40(sp)
    fld     fa7, 32(sp)
    fld     ft8, 24(sp)
    fld     ft9, 16(sp)
    fld     ft10, 8(sp)
    fld     ft11, 0(sp)
    addi    sp, sp, 320
.endm


supervisor_trap_dispatcher:
    # We need to switch stacks if coming from usermode
    # If we are, sscratch will contain a kernel stack. Otherwise, it'll be 0.
    # See docs/RISC-V-Specifics.md.
    csrrw   sp, sscratch, sp        # Swap sp and sscratch
    bnez    sp, _from_user_mode

    # If sscratch was zero, we were already on a kernel stack, so
    # switch back to it (and leave it in sscratch too) then just run
    # the handler and sret
    csrr    sp, sscratch            # sp back to kernel stack

    _trap_dispatcher_impl 0

    sret                            # and back to kernel mode

_from_user_mode:
    # Else sscratch was non-zero, i.e. contained a kernel stack, meaning
    # we came from user-mode. That kernel stack is now in sp, and the user
    # stack is in sscratch.
    #
    # We need to run the handler, and then swap the stacks back before
    # returning to user mode so that the kernel stack will again be in
    # sscratch next time we get here.

    _trap_dispatcher_impl 1

    csrrw   sp, sscratch, sp        # kernel sp into sscratch, user sp into sp
    sret
