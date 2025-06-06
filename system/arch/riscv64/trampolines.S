# Server-loader trampoline (RISC-V)
# anos - An Operating System
#
# Copyright (c) 2025 Ross Bamford
#
# This is just here to sort out the stack pointer and executable
# names for the initial_server_loader, so we can load the exec
# and then reset the stack later.
#

    .globl initial_server_loader
    .option norvc
    .align 2

    .extern initial_server_loader_bounce

initial_server_loader:
    # Pass current stack pointer (sp) in a0
    mv      a0, sp

    # Load argv[0] (at sp + 4 * 8) into a1
    # Each entry is 8 bytes (uintptr_t), and we're interested in the 5th (index 4)
    ld      a1, 32(sp)

    # Jump to bounce
    tail    initial_server_loader_bounce
