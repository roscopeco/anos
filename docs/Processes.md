# Processes in Anos

## Process Start-up Sequence

Starting a new process in Anos (and in pretty-much _any_ OS) is a 
slightly convoluted process, involving a delicate dance between:

1. The kernel
2. SYSTEM
3. The process requesting the start-up
4. The new process being started

> [!NOTE]
> During system start-up, SYSTEM may actually _be_ the one that's
> starting the new process, so in that case 2. and 3. are the same.

This dance involves a number of different system calls, and careful
use of different capability contexts to ensure that the new process,
while having its own strictly-defined set of capabilities (which may
not include disk access or memory mapping, for example) can still be
loaded and set up in its initial environment properly.

### Requesting a new process

Anos is not a POSIX system, nor does it aim to emulate a UNIX - and
one important area where it differs significantly is in process 
creation and management. 

The traditional `fork` and `exec` family of calls are not supported.
Instead, there are two primary means by which processes can be
started:

* The `anos_create_process` system call (for privileged processes)
* The `SYSTEM::PROCESS` IPC namespace (for normal processes)

We'll break down the details of both flows, starting with the simpler
view from the less-privileged client perspective, and then following
up with the more-detailed view from the SYSTEM perspective.

#### Less-privileged (user) process startup flow

> [!WARNING]
> At the time of writing, this part of the flow is not yet 
> implemented, so the details below are subject to change.
> 
> That said, this is only really the beginning of the _actual_ 
> flow - which _is_ already implemented, so if you're interested
> in the nitty-gritty details of how that all works, skip ahead
> to the _More-privileged_ flow, below.

Typically, process creation will be handled via IPC with SYSTEM, 
and most processes will not have the capability to call 
`anos_create_process` directly, but both flows will be described
in this document (since in any case SYSTEM will itself use the 
`anos_create_process` call to do the actual work on behalf of clients).

From the user perspective (i.e. the point of view held by most 
regular client programs in Anos), the general startup flow is 
quite simple:

![Process Startup - User Perspective](../images/diagrams/Process%20Creation%20-%20User%20Perspective.svg)

The user program is shielded from most of the intricacies of
starting a new process, and does not need to concern itself 
with details such as executable formats or memory layouts.

Instead, the user program simply sends a message to SYSTEM via
standard IPC channels with the following details:

* The process file identifier
* Capabilities it would like to delegate to the new process
* Command-line arguments and environment settings

From there, SYSTEM will use its capabilities to communicate to
the kernel that a new process is required, and will then 
collaborate with both the kernel and with the new process
itself to bring it to a state where it is ready to run 
its own code.

#### More-privileged (SYSTEM) process startup flow

From the point of view of SYSTEM, the flow is significantly
more complicated than the view presented to user programs, 
but the end goal is (of course) the same - to get a new 
process **loaded into memory**, make sure it has the **resources
it needs to run**, and that the thread scheduler is aware of
it and **gives it appropriate CPU time**.

Whether a new process is being started as a result of an IPC
message (`TODO`) as described above or because SYSTEM itself
needs to spawn a new process directly, the steps from here on
out are the same.

Briefly, we need to:

* Have the kernel create a new process for us
* Have the kernel create a new task, owned by that process
* Load some code into the new process' address space
* Set up the new process' memory in the way it expects
* Ensure everything is set up so that the new process receives CPU time

The way this works in Anos is designed around the core 
design principles in the [Three Pillars](ThreePillars.md).

In more detail, the flow looks like this:

![Detailed Process Startup Flow](../images/diagrams/Process%20Creation%20-%20System%20Perspective.svg)

### Initial Stack Layout

> [!NOTE]
> The kernel is entirely non-opinionated when it comes to 
> process stack layout - it's purely a userspace concern. 
> 
> The information here documents how SYSTEM approaches it, in 
> concert with the standard Anos Toolchain's `crt0.asm` and
> standard library code.
> 
> The kernel plays a supporting role in ensuring the stack is
> copied into the new process address space, but the _contents_
> of that copy are completely controlled by SYSTEM.

When a new process is created, SYSTEM uses a standard stack layout
to pass required information to it:

* Capabilities
* Command-line

The initial process stack looks like this:

<img alt="New Process - Initial Stack Layout" src="../images/diagrams/New%20Process%20-%20Initial%20Stack%20Layout.svg">

SYSTEM ensures that when the new process' entrypoint is called,
the stack is set up like this, and the stack pointer is set up
correctly.

The standard library's `crt0` then pops the values for the pointers
as appropriate, setting capabilities up in an internal capability
map (where the `anos_` syscall routines will look for them) and
passing `argc` and `argv` into the `main` function as you 
probably expect.





