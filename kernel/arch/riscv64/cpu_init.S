# stage3 - RISC-V CPU initialization
# anos - An Operating System
#
# Copyright (c) 2024 Ross Bamford

.section .text
.global cpu_init_tp

# Maximum number of CPUs supported
.equ MAX_CPUS, 16

# cpu_init_tp(void)
# Sets up tp register to point to this CPU's state
cpu_init_tp:
    # Get hart ID
    csrr t0, mhartid    # We can still read mhartid in S-mode
    
    # Check if hart ID is within bounds
    li t1, MAX_CPUS
    bgeu t0, t1, .invalid_hart
    
    # Calculate offset into cpu_state array
    slli t0, t0, 10     # Multiply by 1024 (size of per-CPU state)
    
    # Load base address of cpu_state array
    la t1, cpu_state
    
    # Add offset to base
    add t0, t0, t1
    
    # Set tp to point to this CPU's state
    mv tp, t0
    
    ret

.invalid_hart:
    # If we get here, the hart ID is out of bounds
    # We could either:
    # 1. Halt this CPU
    # 2. Set tp to a default value
    # 3. Jump to some error handler
    # For now, we'll just halt this CPU
    wfi                 # Wait for interrupt (effectively halts the CPU)
    j .invalid_hart    # In case we wake up, loop forever 