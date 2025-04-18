# stage3 - Kernel initializer for Limine boot protocol on riscv64
# anos - An Operating System
#
# Copyright (c) 2025 Ross Bamford
#

# We don't want compressed insns...
.option norvc

.section .text.init

# Kinda dodgy, but we'll only hit it on early exception...
# It should panic really, but for now it just halts.
s_trap_vector:
1:
	# Disable interrupts
	csrw	sie, zero
	csrw	sip, zero

	# Halt
	wfi
	j 1b

.global _start_limine
_start_limine:

	# Set up GP first of all, limine leaves it zeroed...
.option push
.option norelax
	la		gp, _global_pointer

	la		t2, s_trap_vector
	csrw	stvec, t2
.option pop
	# Zero BSS...
	la 		a0, _bss_start
	la		a1, _bss_end
	bgeu	a0, a1, 2f
1:
	sd		zero, (a0)
	addi	a0, a0, 8
	bltu	a0, a1, 1b
2:
	# Setup stack and let's get to it!
	la		sp, _stack_end
	tail 	bsp_kernel_entrypoint_limine


# `limine_entrypoint.c` calls back to this to get the page tables and stack sorted 
# out without upsetting C too much.
#
# Arguments (mostly passed through):
#
#   a0 - framebuffer width
#   a1 - framebuffer height
#   a2 - virtual address for new stack top
#   a3 - physical address and mode of new PML4
#   a4  - address to bounce off to
#
.global bootstrap_trampoline
bootstrap_trampoline:
	mv t0, a3
    csrw satp, t0		# Set up new page tables
    # mv sp, a2			# Set up new stack
    mv t0, a4			# push return address
    jr t0				# Return (equivalent to ret)
