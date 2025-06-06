# Implementation details specific to RISC-V

## User / Kernel Stack Switch

x86_64 gets a lot of hate for its complexity, but some of that complexity
is actually useful. One of the things I missed when starting the RISC-V 
port was the ability to use the TSS to automatically switch stacks when 
going between privilege levels (and even for different 
exceptions / interrupts).

RISC-V lacks any automatic support for that, so we have to do things
more manually, but luckily there are some nice features that make it 
not too painful to manage, once you wrap your head around how it works.

The slight complication is (of course) the fact that we need to switch
stacks before we can save any context, and we need to not stomp on any
registers at that point. There's nowhere to store them (since we can't
trust the user-mode `sp` and we want this to be nestable).

The method STAGE3 uses for this on RISC-V is broadly the same as the 
way Linux does it - by using `sscratch` as storage for a kernel-mode 
stack when in user mode, and using `csrrw` to switch the stacks on
entry to the ISR dispatcher.

### The basic scheme

* When in user-mode, `sscratch` will always contain a kernel stack
* When in kernel-mode, `sscratch` will always be zero
* When entering the trap dispatcher:
  * Atomically swap `sp` and `sscratch`
  * Is `sp` now zero?
    * If **yes** - we were in kernel mode, swap them back and carry on
    * If **no** - we were in user mode, kernel stack is now in `sp` and user stack is in `sscratch`
  * Either way, we now have a kernel stack in `sp` so can proceed to save context and run the handler...
* When exiting the trap dispatcher:
  * If we came from user-mode originally, swap `sscratch` and `sp` again after restoring registers

### Other considerations

This does mean we need to ensure we initialize `sscratch` to zero during 
kernel init, which is no real hardship.

We also need to ensure that the places where usermode is entered or exited
all account for the fact they need to put a kernel stack in `sscratch`.

> ![WARNING]
> The way thread stacks are handled on RISC-V means that some of the fields 
> in the `PerCPUState` and `Task` structs aren't doing the same things they
> do on x86_64 - and may be counterintuitively named.
> 
> Basically, expect some weirdness in this area on RISC-V while this is
> all being settled.


