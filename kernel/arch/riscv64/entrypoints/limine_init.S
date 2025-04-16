# stage3 - Kernel initializer for Limine boot protocol on riscv64
# anos - An Operating System
#
# Copyright (c) 2025 Ross Bamford
#

# We don't want compressed insns...
.option norvc

.section .text.init

# Kinda dodgy, but we'll only hit it on early exception...
# It should panic really...
s_trap_vector:
	sret

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
