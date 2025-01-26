## anos - An Operating System 💾

![workflow status](https://github.com/roscopeco/anos/actions/workflows/compile_test.yaml/badge.svg)
![code coverage](coverage.svg)

> **Note**: This is not yet an operating system, but _has_ just about 
> reached toy status, since it now supports user mode preemptive 
> multitasking.

A toy operating system I'm using as a vehicle for learning more about
long mode and experimenting with some ideas for different ways to do 
things.

I'm _trying_ to not even _look_ at other people's code, or patchy and 
outdated wikis etc - as much as possible I'm just going from official 
reference materials (like the Intel manuals).

  - Updated mission statement, January 2025

### High-level overview

> **Note** this is still evolving!

Right now, there's a two-stage bootloader (I know there are existing,
better options, but one of my goals is to figure legacy bootloaders out)
that will load from FAT floppy (and only floppy - no hard-disk support yet).

The bootloader does enough basic set up to load a (flat binary, ELF might
be supported eventually) kernel at 0x120000 (physical), set up long mode
with initial (super basic) paging, and call that kernel at 
0xFFFFFFFF80120000 (virtual, mapped to the physical load address).

The loader also fetches a BIOS memory map before leaving (un)real mode
forever, and passes that map (unprocessed, currently) to the kernel via a 
pointer parameter.

The loader **does not** do any BSS clearing or other C set-up - the kernel
has a small assembly stub that just zeroes out that segment and passes
control. No (C++, or C with GCC extensions) static constructors or anything
else are run at the moment - this is not a complete C environment :D

Also worth noting that the loader doesn't do **anything** at all with 
interrupt vectors - that's totally up to the kernel (and interrupts are
left disabled throughout the load and remain so on kernel entry).

As far as the Kernel is concerned, there's much to still be decided,
and most of what has been decided could still change without notice.
However, since this is being designed as 64-bit from the beginning, there's
a lot of things I can do that I wouldn't otherwise be able to, and
decisions I've taken so far are, as much as possible, documented in
the source / comments.

Right now, it's early days, but there's a simple stack-based physical
memory allocator and enough support for everything (page fault handling,
IDT, virtual memory management, etc) to be able to get to user mode
and then back via a simple syscall interface (accessible via both
`int` and `syscall` interfaces).

The basics of interrupt handling is configured, with the local APIC 
timer currently providing a "proof-of-life" heartbeat and basic 
multitasking via the round-robin scheduler.

### Building

Everything is built with `make`. You'll want a cross-compiler
toolchain for `x86_64` (for `ld` and `gcc` plus supporting things)
along with cross `binutils` (for `objcopy` and `objdump` at least). 

Latest versions of these are recommended.

All assembly code is built with NASM. 2.16 or higher is recommended.

For running and debugging, you'll want `qemu-system-x86_64`. 
Bochs is also supported if you prefer (there's a minimal `bochsrc`
in the repo that will get you going).

To build the FAT filesystem for the floppy image, `mtools` is
needed, specifically `mformat`, `mcopy` - but I don't know if 
you can get just those, and it's best to get the whole suite
anyway as it can be useful for debugging things.

You'll need a sane build environment (i.e. a UNIX) with `make` 
etc. FWIW I work on macOS, YMMV on Linux or WSL (but I expect
it should work fine).

There are probably some test programs and helpers that I use 
in the repo (e.g. `fat.c`). These will need a native Clang or GCC
toolchain - again, sane build environment recommended... 😜

To build, just do:

```shell
make clean all
```

This will do the following:

* Build kernel ELF
* Build the user-mode `libanos` and `System` user-mode supervisor
* Create a disassembly file (`.dis`)
* Build a floppy-disk image with the (stripped) kernel and System
* Run a bunch of unit tests

You can also choose to just run `make test` if you want to run the
tests. If you have `LCOV` installed, you can also generate 
coverage reports with `make coverage` - these will be output in
the `gcov/kernel` directory as HTML.

### Running

#### In an emulator

You can use either qemu or Bochs. Obviously you'll need them
installed. 

> **Note** If you're on Mac and want to use Bochs, it's best to 
> build your own from source. The one in brew is kinda broken, 
> in that the display doesn't always work right and the debugger
> has bad keyboard support (no history etc).

To run in qemu:

```shell
make qemu
```

Or Bochs:

```shell
make bochs
```

This latter one is really just running `bochs` directly, but will
also handle building the code and floppy image automatically for 
you. Of course, you can just run `bochs` directly yourself if 
you like - I'm not one to judge.

#### On real hardware

If you want to run this on real hardware, you'll need something
that will either write the `img` file to a floppy as raw sectors,
or something that can burn bootable USB sticks.

Full disclosure, I haven't tested this on actual hardware yet, 
and to be honest I'd be pretty surprised if it worked. I _think_
I've got all the basics covered (retrying disk accesses when they
fail and enabling A20 are both unnecessary in the emulators, but 
the code is written and tested as much as I possibly can) but 
there's every chance things won't work right anyway...

### Debugging

The recommended way to debug is with qemu. Bochs _is_ still supported,
and the debugger built into it isn't _bad_, but full-fat GDB is pretty
hard to beat and it works well with qemu.

For convenience, a `.gdbinit` file is provided that will automate
loading the symbols and connecting to qemu. This can be easily
kicked off with:

```shell
make debug-qemu
```

This will build what needs to be built, start qemu with debugging,
and launch GDB automatically.

> **Note** You'll want NASM 2.16 or higher if you have a modern
> GDB (13 and up) - I've observed issues with loading DWARF data
> generated by NASM 2.15, which causes symbol clashes and sources 
> not to be loaded.

Because symbols and sources are loaded, you can set breakpoints 
easily based on labels or line numbers, e.g:

```gdb
b stage2.asm:_start
b stage1.asm:35
```

If you prefer to use debugging in an IDE or have some other alternative
GDB frontend you like to use, you can just run:

```shell
make debug-qemu-start
```

which will skip starting GDB for you, allowing you to launch 
your frontend and connect (`localhost:9666` by default).

#### Debugging in VSCode

If Visual Studio Code is your bag, you should be able to debug visually using that,
there are `launch.json` and `tasks.json` files included that seem to work on 
my machine. 

You'll probably need the "Native Debugging" extension installed from the marketplace,
the standard one seems to be unusually braindead, even by Visual Studio standards.

Once you have that, stick a breakpoint in the margin somewhere and then run the 
`(gdb) Attach` configuration from the debug tab. It should build the project,
kick off qemu and then connect to it allowing you to debug things.

> **Note**: Certain things don't work well, I'd still go with `gdb` terminal if
> you're comfortable with it. The stack doesn't get populated correctly in vscode,
> for example - though it _does_ use the symbols we give it so it can at least find
> the appropriate line of code in both C and assembly source, so it's _basicaly usable_.

### Developing

If you're developing the code, you'll want to have
[clang-format](https://clang.llvm.org/docs/ClangFormat.html) installed for code formatting.

If you're committing to git, we use [pre-commit](https://pre-commit.com) to manage 
pre-commit hooks that handle formatting automatically on commit.

```bash
pip install pre-commit
pre-commit install
```

### Recent screenshot

This probably isn't up to date enough to represent where it's at, but
it should be relatively recent.

* Boot from real to long mode
* Set up a RLE stack-based PMM
* Set up VMM & recursive paging (for now, will likely change later)
* Set up fixed block & slab allocators
* _Just enough_ ACPI to initialise basic platform devices (HPET & LAPICs)
* Init LAPICs and calibrate with HPET
* Set everything up for usermode startup
* Start a simple round-robin scheduler & drop to user mode
* User-mode supervisor ("`SYSTEM`") starts some threads with a `syscall`, and then
  * Thread #1 - Loop from usermode, printing periodic `1`'s with syscall
  * Thread #2 - Start, sleep for 5secs, then loop printing periodic `2`s with syscall 
  * Thread #3 - Same as thread 2, but printing `3`s instead
  * Thread #4 - Looping as the others, but also sleeping every time it prints

It's running in VirtualBox here, just for a change from qemu...

<img src="images/Screenshot 2025-01-26 at 12.06.41.png" alt="ANOS running in VirtualBox">

The following things are not shown in this shot, but are still happening under the hood:

* Enumerate PCI bus (including bridges)

