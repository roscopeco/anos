## Memory Map for Anos

### Low Memory

Various bits of low memory are used. Some of them are only used
during the boot process and are reused later.

During early boot (until the kernel sets up proper page tables for
the first time) there's some risky bits - stacks are _theoretically_
unbounded for example (but in reality never grow down beyond maybe
128 bytes or so at maximum).

The following are **physical** addresses:

| Start                | End                  | Use                                                    |
|----------------------|----------------------|--------------------------------------------------------|
| `0x000000000009c000` | `0x000000000009cfff` | Inital PML4 (Becomes `SYSTEM` process table post-boot) |
| `0x000000000009d000` | `0x000000000009dfff` | Inital PDPT (Ends up owned by `SYSTEM` process)        |
| `0x000000000009e000` | `0x000000000009efff` | Inital PD (Ends up owned by `SYSTEM` process)          |
| `0x000000000009f000` | `0x000000000009ffff` | Inital PT (Ends up owned by `SYSTEM` process)          |

### Kernel Physical Mem (after bootloader & limine_entrypoint / trampoline is done)

These memory areas are reserved, and not added into the pool managed by the PMM. There are some additional
structures and regions used by the bootloader that are passed to the kernel dynamically - for the most part
these are reclaimed after the entrypoint trampoline has been bounced.

| Start                | End                  | Use                                                             |
|----------------------|----------------------|-----------------------------------------------------------------|
| `0x0000000000099000` | `0x0000000000099fff` | PMM Bootstrap page (Region struct and bottom of stack)          |
| `0x000000000009a000` | `0x000000000009afff` | PMM Area bootstrap page directory                               |
| `0x000000000009b000` | `0x000000000009bfff` | PMM Area bootstrap page table                                   |
| `0x000000000009c000` | `0x000000000009cfff` | Inital PML4                                                     |
| `0x000000000009d000` | `0x000000000009dfff` | Inital PDPT                                                     |
| `0x000000000009e000` | `0x000000000009efff` | Inital PD                                                       |
| `0x000000000009f000` | `0x000000000009ffff` | Inital PT                                                       |
| `0x0000000000100000` | `0x000000000010ffff` | Initial 64KiB long-mode stack (set up by bootloader for kernel) |
| `0x0000000000110000` | `0x000000000011ffff` | 64KiB Kernel BSS                                                |
| `0x0000000000120000` | `0x00000000001fffff` | 896KiB Kernel load area                                         |

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
in step, or you're likely to experience sadness).

### Long Mode Initial Page Table Layout

Once long mode gets set up, the platform- and loader-specific bootstraps
set up a few mappings in the initial page tables:

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0x0000000000000000` | `0x0000000000200000` | Identity mapped to physical bottom 2MiB (**x86_64 only**)    |
| `0xffffffff80000000` | `0xffffffff803fffff` | Kernel code / data static mapping (4MiB)                     |
| `0xffffffff82000000` | `0xffffffff827fffff` | Framebuffer (**UEFI only**, all platforms)                   |

This mapping lets the long mode setup keep running as it was (in the identity-mapped space),
up until it jumps directly to the kernel at it's (virtual) load address (in the kernel space
mapping). This happens at the end of the platform-specific bootstrap.

> [!WARNING]
> On **x86_64 (only)**, the second 2MiB of physical is mapped into kernel space for 
> convenience, to make it easy to access the first 2MiB of RAM that's managed by the
> PMM during init.
>
> The first 2MiB is excluded from the PMM, so is manually managed. This is just because
> there's already a kernel and various data (page tables etc.) there by the time the PMM
> is getting initialized, and stomping on them would be **bad** 😱

### Long Mode Page Table Layout After Kernel Bootstrap

> [!NOTE]
> From this point on, the map is the same regardless of how we were booted.
> 
> On x86_64, once the entrypoint has exited, and we're in the kernel proper, the identity 
> mapping of low RAM is dropped (the first 2MB physical mapping into the top 2GiB 
> is retained, and for UEFI boot we currently copy the kernel to that physical 
> space).
>
> On RISC-V, because we have no guarantees about physical memory, we instead keep
> the kernel at its load address, copy the bootloader mappings into our pagetables,
> and exclude `EXECUTABLE_AND_MODULES` memory regions from the PMM.

We also create the mapping for the 128GiB reserved address space for the PMM
structures at this point, leaving us with:

| Start                | End                  | Use                                                           |
|----------------------|----------------------|---------------------------------------------------------------|
| `0x0000000000000000` | `0x00007fffffffffff` | User space (128TiB)                                           |
| `0x0000800000000000` | `0xffff7fffffffffff` | [_Non-canonical memory hole, almost 16EiB_]                   |
| `0xffff800000000000` | `0xfffffeffffffffff` | Virtual Mapping area (127TiB) (see below)                     |
| `0xffffff0000000000` | `0xffffff7fffffffff` | [_Currently unused, 512GiB_]                                  |
| `0xffffff8000000000` | `0xffffff9fffffefff` | PMM structures area (only the first page is actually present) |
| `0xffffff9ffffff000` | `0xffffff9fffffffff` | PMM structures guard page (Reserved, never mapped)            |
| `0xffffffa000000000` | `0xffffffff7fffffff` | [_Currently unused, 382GiB_]                                  |
| `0xffffffff80000000` | `0xffffffff803fffff` | Kernel code / data static mapping (4MiB)                      |
| `0xffffffff80400000` | `0xffffffff80ffffff` | 256 per-CPU temporary mapping pages (**see notes, below**)    |
| `0xffffffff81000000` | `0xffffffff8101ffff` | [_Currently unused, 128KiB_]                                  |
| `0xffffffff81020000` | `0xffffffff81020fff` | Kernel driver LAPIC mapping page                              |
| `0xffffffff81021000` | `0xffffffff810fffff` | Kernel driver MMIO mapping space (892KiB, 223 pages)          |
| `0xffffffff81100000` | `0xffffffff81ffffff` | (Temporary) ACPI tables (Early boot only, 15MiB)              |
| `0xffffffff82000000` | `0xffffffff827fffff` | Bootup terminal framebuffer (8MiB)                            |
| `0xffffffff82800000` | `0xffffffffbfffffff` | [_Currently unused, 984MiB_]                                  |
| `0xffffffffc0000000` | `0xffffffffffffffff` | 1GiB FBA space                                                |

#### Notes on specific areas

##### Virtual mapping area

We reserve the first available 127TiB of kernel space for the virtual mapping area. 
This area covers 254 PML4 entries from 256 to 509, leaving one entry (510) unused
and available for per-platform requirements, with the last entry (covering 512GB)
free for the rest of kernel space.

###### x86_64

On x86_64, this area holds a 1:1 direct map of all physical RAM in the system, as
detected at boot. This region is used by the virtual-memory subsystem for pagetable
management, and can also be used by other subsystems that need to directly 
maniu

The downsides of course are that all physical memory is mapped statically in
the kernel. It also means we have to maintain tables for all of these mappings
(but we do endeavour to use the largest possible page mappings for these areas
to minimise the overhead).

###### RISC-V

RISC-V page tables are not amenable to recursive mapping, since a table entry
cannot be both an intermediate and leaf table at the same time, so on this 
platform the design was always going to be direct-mapping.

##### ACPI tables

> [!WARNING]
> This section applies to **x86_64 only**.

> [!NOTE]
> This section remains accurate on UEFI systems (booted via Limine) - we drop Limine's
> mappings super early and remap these tables according to our own scheme.
>
> _However_, if you're looking at very early entrypoint code, it might be worth noting
> that the ACPI tables won't be mapped in this region until after the ACPI init routines
> have been called...

ACPI tables are mapped into a small (8-page currently) reserved space. Once usermode is
up this will be remapped read-only, and made available for mapping into the userspace
driver management subsystem's address space via a syscall.

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0xffffffff81000000` | `0xffffffff81008000` | Reserved space for ACPI tables                               |

Probably worth noting that these aren't moved or copied - they stay at their original 
phys location, and I just map it in. Maybe a bad idea, but works for now (on emus).

##### Per-CPU temporary mapping pages

Each CPU (up to a maximum of 256) has a single 4KiB page in this area that can be used for
a temporary mapping (e.g. when a single page needs to be copied without mapping in a whole
other address space).

Each CPU's page is at `0xFFFFFFFF80400000 + (CPU_ID << 12)`. There are no spinlocks 
protecting these (since they're per-CPU) but they should only be used with local
interrupts disabled for obvious reasons.

This is currently used for mapping new pages for COW pages in the pagefault handler.

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0xffffffff80400000` | `0xffffffff80ffffff` | 256 Per-CPU temporary mapping pages                          |

This space is not represented by page tables at boot, but if there's a page
fault there the handler will automatically map in a page. 

##### Hardware mappings

> [!WARNING]
> This section applies to **x86_64 only**. It needs to be updated for RISC-V
> as the implementation progresses.

The local APIC is mapped to:

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0xffffffff81020000` | `0xffffffff81020fff` | Local APIC (for all CPUs)                                    |

While the immediately following region is reserved for kernel driver 
MMIO mapping:

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0xffffffff81021000` | `0xffffffff810fffff` | Kernel driver MMIO mapping space                             |

This area (of 223 pages, or 892KiB) is specifically reserved for mapping the
basic kernel drivers (the ones in `kdrivers/`). Allocation is handled in 
`kdrivers/drivers.c` with a simple check and increment - there's no ability
to free pages here (because there's no expectation we'll ever unmap these 
basic drivers once they're mapped in).

#### Kernel memory areas

The following areas are reserved for allocation of various forms within 
the kernel.

##### Fixed-block space

The following space is reserved for the fixed block allocator (and is where
the slab allocator will map blocks).

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0xffffffffc0000000` | `0xffffffffffffffff` | 1GiB FBA space                                               |

The fixed-block allocator works on 4KiB page granularity, so supports a 
maximum of 262144 blocks within this 1GiB space. Of those, the first eight
are used for metadata (a 32KiB bitmap tracking virtual address allocation
for this area) meaning there can be a maximum of 262136 blocks mapped into
this virtual area (the first eight are of course always mapped).

A gigabyte _feels_ like a lot, but we have the address space and I don't
feel like painting myself into any corners just now...

#### Notes / Future Plans

> [!WARNING]
> This section applies to **x86_64 only**.

This is all fun and games, but will almost certainly be a problem later 
(i.e. the kernel being stuck at <1MiB size and located at the ~1MiB mark in physical 
RAM on x86_64).

In the future it'll make sense to make things more robust, and maybe 
just load the kernel somewhere above 16MiB physical and map accordingly
(Kernel space can still map broadly the same - it might be useful to have
the first 16MiB mapped at the start of kernel space 🤔).

_Maybe_ I'd check we actually have >16MiB first, but are there even any 
x86_64s that have less RAM than that? 🤷

This is essentially what we currently do for RISV-V, so it should be no
real hardship to replicate that for x86_65 (especially now we have 
direct virtual mapping instead of recursive).
