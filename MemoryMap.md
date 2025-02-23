## Memory Map for Anos

> [!NOTE]
> The low memory and early boot information here applies **only when booted via STAGE2**.
> When booted with Limine (or other such loader as we may support from time to time)
> the kernel entrypoint code for that loader is responsible for making the bootloader's
> environment into something palatable for the kernel proper.

### Low Memory

Various bits of low memory are used. Some of them are only used
during the boot process and are reused later.

Some of these can be changed at build time (with e.g. `STAGE_2_ADDR`
defines) but probably shouldn't unless you know what you're doing
and have reasons for doing it...

During early boot (until the kernel sets up proper page tables for
the first time) there's some risky bits - stacks are _theoretically_
unbounded for example (but in reality never grow down beyond maybe
128 bytes or so at maximum).

The following are **physical** addresses:

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `------------------` | `0x0000000000007bff` | Stage 1/2 (un)real mode stack (actually only about 20 bytes) |
| `0x0000000000007c00` | `0x0000000000007dff` | Bootsector / Stage 1                                         |
| `0x0000000000007e00` | `0x000000000000a200` | FAT buffer (only during stage 1 file search)                 | 
| `0x0000000000008400` | `0x0000000000009bff` | BIOS E820h memory map (passed to kernel by stage2)           |
| `0x000000000000a400` | `0x000000000000ffff` | Stage 2 load area (max 23k, doesn't use it all)              |
| `------------------` | `0x000000000009bfff` | Stage 2 Protected stack (probably pointless moving it here..)|
| `0x000000000009c000` | `0x000000000009cfff` | Inital PML4 set up in stage2 init_pagetables.asm             |
| `0x000000000009d000` | `0x000000000009dfff` | Inital PDPT set up in stage2 init_pagetables.asm             |
| `0x000000000009e000` | `0x000000000009efff` | Inital PD set up in stage2 init_pagetables.asm               |
| `0x000000000009f000` | `0x000000000009ffff` | Inital PT set up in stage2 init_pagetables.asm               |

### Kernel Physical Mem (after stage2 is done)

These memory areas are reserved, and not added into the pool managed by the PMM.

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0x0000000000008400` | `0x0000000000009bff` | BIOS E820h memory map (passed to kernel by stage2, SEE BELOW)|
| `0x0000000000099000` | `0x0000000000099fff` | PMM Bootstrap page (Region struct and bottom of stack)       |
| `0x000000000009a000` | `0x000000000009afff` | PMM Area bootstrap page directory                            |
| `0x000000000009b000` | `0x000000000009bfff` | PMM Area bootstrap page table                                |
| `0x000000000009c000` | `0x000000000009cfff` | Inital PML4 set up in stage2 init_pagetables.asm             |
| `0x000000000009d000` | `0x000000000009dfff` | Inital PDPT set up in stage2 init_pagetables.asm             |
| `0x000000000009e000` | `0x000000000009efff` | Inital PD set up in stage2 init_pagetables.asm               |
| `0x000000000009f000` | `0x000000000009ffff` | Inital PT set up in stage2 init_pagetables.asm               |
| `0x0000000000100000` | `0x000000000010ffff` | Initial 64KiB long-mode stack (set up by stage 2 for kernel) |
| `0x0000000000110000` | `0x000000000011ffff` | 64KiB Kernel BSS                                             |
| `0x0000000000120000` | `0x00000000001fffff` | 896KiB Kernel load area                                      |

The `0x8400-0x9bff` space is reused for the AP startup trampoline, which is done after the memory map has
been processed during early startup. Once AP startup is in progress, that area looks like:

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0x0000000000001000` | `0x0000000000004fff` | AP trampoline bootstrap code (real -> long mode)             |
| `0x0000000000005000` | `0x0000000000005fff` | AP trampoline Data / BSS - kernel passes data to and from    |
| `0x0000000000006000` | `0x00000000000d5fff` | AP initial stacks (2KiB each AP for now)                     |

This area remains reserved until AP startup is fully complete and the scheduler has been initialized on
all APs - until that time, this area will still be in use for stacks (at least).

These ranges are defined in `realmode.ld` (and repeated in `startup.c` so if you change them, keep them
in step or you're likely to experience sadness).

### Long Mode Initial Page Table Layout

Once long mode gets set up, we set up two mappings in the 
initial page tables:

* `0x0000000000000000` -> `0x0000000000200000` : Identity mapped to physical bottom 2MiB
* `0xffffffff80000000` -> `0xffffffff801fffff` : First 2MiB of top (or negative) 2GiB mapped to first 2MiB phys

This mapping lets the long mode setup keep running as it was (in the 
identity-mapped space), up until it jumps directly to the kernel at it's
(virtual) load address (in the kernel space mapping).

> **Note**: The second 2MiB of physical is mapped into kernel space for convenience,
> to make it easy to access the first 2MiB of RAM that's managed by the PMM during init.
>
> It likely won't stay...
>
> The first 2MiB is excluded from the PMM, so is manually managed. This is just because
> there's already a kernel and various data (page tables etc) there by the time the PMM
> is getting initialized, and stomping on them would be **bad** 😱

### Long Mode Page Table Layout After Kernel Bootstrap

> [!NOTE]
> From this point on, the map is the same regardless of how we were booted.

Once stage 2 has exited and we're in the kernel proper, the identity mapping of low RAM 
is dropped (the first 2MB physical mapping into the top 2GiB is retained).

We reserve the first available 127TB of kernel space for the virtual mapping area. 
This area covers 255 PML4 entries from 256 to 510, leaving the last entry (512GB) free for 
the rest of kernel space. This area _could_ be used for a recursive mapping (with a 
randomized recursive entry if I felt like it) but ultimately I'll probably just
do like Linux used to (on 32-bit) and have huge-page mappings for all of physical RAM
in this region, which will simplify quite a few things...

We also create the mapping for the 128GiB reserved address space for the PMM structures
at this point, leaving us with:

* `0x0000000000000000` -> `0x00007fffffffffff` : User space
* `0x0000800000000000` -> `0xffff7fffffffffff` : [_Non-canonical memory hole_]
* `0xffff800000000000` -> `0xffffff7fffffffff` : Virtual Mapping area (127TiB)
* `0xffffff8000000000` -> `0xffffff9fffffefff` : PMM structures area (only the first page is actually present).
* `0xffffff9ffffff000` -> `0xffffff9fffffffff` : PMM structures guard page (Reserved, never mapped)
* `0xffffffa000000000` -> `0xffffffa0000003ff` : Local APIC (for all CPUs) [TODO should probably move to driver space]
* `0xffffffa000000400` -> `0xffffffff7fffffff` : [_Currently unused, ~381GiB_]
* `0xffffffff80000000` -> `0xffffffff803fffff` : First 4MiB of top (or negative) 2GiB mapped to first 4MiB phys (kernel code etc is here!)
* `0xffffffff80400000` -> `0xffffffff80ffffff` : 1MiB Kernel "Automap" space, **for testing only**
* `0xffffffff81000000` -> `0xffffffff8101ffff` : (Temporary) Reserved space for ACPI tables
* `0xffffffff81020000` -> `0xffffffff810fffff` : Kernel driver MMIO mapping space (895KiB, 224 pages)
* `0xffffffff81100000` -> `0xffffffff81ffffff` : [_Currently unused, 15MiB_]
* `0xffffffff82000000` -> `0xffffffff823fffff` : Bootup terminal framebuffer (4MiB)
* `0xffffffff82400000` -> `0xffffffffbfffffff` : [_Currently unused, 988MiB_]
* `0xffffffffc0000000` -> `0xffffffffffffffff` : 1GiB FBA space

#### Notes on specific areas

ACPI tables are mapped into a small (8-page currently) reserved space, which is _probably_
going to be just temporary (just during bootstrap) - not sure there's much point in keeping
it around once the devices are setup 🤔:

* `0xffffffff81000000` -> `0xffffffff81008000` : Reserved space for ACPI tables

Probably worth noting that these aren't moved or copied - they stay at their original 
phys location, and I just map it in. Maybe a bad idea, but works for now (on emus).

Additionally, the debug page fault handler manages this:

* `0xFFFFFFFF80400000` -> `0xFFFFFFFF80FFFFFF` : Kernel "Automap" space, **for testing only**

This space is not represented by page tables at boot, but if there's a page
fault there the handler will automatically map in a page. 
**This is just for testing, and is not a feature - it will be removed soon!!**.

The local APIC is **temporarily** mapped to:

* `0xffffffa000000000` -> `0xffffffa0000003ff` : Local APIC (for all CPUs)

This, again, won't be staying there, but it works for now. It'll eventually
get moved into:

* `0xffffffff81008000` -> `0xffffffff810fffff` : Kernel driver MMIO mapping space

This area (of 248 pages, or 992KiB) is specifically reserved for mapping the
basic kernel drivers (the ones in `kdrivers/`). Allocation is handled in 
`kdrivers/drivers.c` with a simple check and increment - there's no ability
to free pages here (because there's no expectation we'll ever unmap these 
basic drivers once they're mapped in).

#### Notes / Future Plans

This is all fun and games, but will almost certainly be a problem later 
(i.e. the kernel being stuck at <1MiB size and located at the ~1MiB mark in physical 
RAM).

In the future it'll make sense to make things more robust, and maybe 
just load the kernel somewhere above 16MiB physical and map accordingly
(Kernel space can still map broadly the same - it might be useful to have
the first 16MiB mapped at the start of kernel space 🤔).

_Maybe_ I'd check we actually have >16MiB first, but are there even any 
x86_64s that have less RAM than that? 🤷

#### Kernel memory areas

The following areas are reserved for allocation of various forms within 
the kernel.

##### Fixed-block space

The following space is reserved for the fixed block allocator (and is where
the slab allocator will map blocks).

* `0xffffffffc0000000` -> `0xffffffffffffffff` : 1GiB FBA space

The fixed-block allocator works on 4KiB page granularity, so supports a 
maximum of 262144 blocks within this 1GiB space. Of those, the first eight
are used for metadata (a 32KiB bitmap tracking virtual address allocation
for this area) meaning there can be a maximum of 262136 blocks mapped into
this virtual area (the first eight are of course always mapped).

A gigabyte _feels_ like a lot, but we have the address space and I don't
feel like painting myself into any corners just now...


