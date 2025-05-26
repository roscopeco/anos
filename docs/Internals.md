# Kernel Internals

> [!NOTE]
> This documentation is under construction, and is evolving with the kernel
> itself. Only things that are somewhat solidified are documented here, 
> and any information is subject to change as the design and implementation
> progresses.

## Kernel Boot Flow

This diagram shows the general code flow for the kernel boot and initialisation
process on x86_64. 

<img alt="Kernel Boot Init flow diagram" src="../images/diagrams/Kernel%20Startup%202025-05-25.svg">

## RAMFS Physical Layout

This diagram shows the physical layout of RAMFS file systems in memory.

<img alt="RAMFS Physical Memory Layout" src="../images/diagrams/RAMFS%20Physical%20Layout.svg">
