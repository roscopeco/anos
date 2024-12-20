## Memory Map for Anos

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
| `0x0000000000008400` | `0x0000000000009bff` | BIOS E820h memory map (passed to kernel by stage2)           |
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

Once stage 2 has exited and we're in the kernel proper, the identity mapping of low RAM 
is dropped (the first 2MB physical mapping into the top 2GiB is retained).

We also create the mapping for the 128GiB reserved address space for the PMM structures
at this point, leaving us with:

* `0xffffff8000000000` -> `0xffffff9fffffefff` : PMM structures area (only the first page is actually present).
* `0xffffff9ffffff000` -> `0xffffffa000000000` : PMM structures guard page (Reserved, never mapped)
* `0xffffffff80000000` -> `0xffffffff80200000` : First 2MiB of top (or negative) 2GiB mapped to first 2MiB phys
* `0xffffffff80200000` -> `0xffffffff803fffff` : Second 2MiB of top (or negative) 2GiB mapped to second 2MiB phys*

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

* `0xFFFF800000000000` -> `0xFFFF800000000400` : Local APIC (for all CPUs)

This, again, won't be staying there, but it works for now.

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

* `0xffffffff90000000` -> `0xffffffffcfffffff` : 1GiB FBA space

The fixed-block allocator works on 4KiB page granularity, so supports a 
maximum of 262144 blocks within this 1GiB space. Of those, the first eight
are used for metadata (a 32KiB bitmap tracking virtual address allocation
for this area) meaning there can be a maximum of 262136 blocks mapped into
this virtual area (the first eight are of course always mapped).

A gigabyte _feels_ like a lot, but we have the address space and I don't
feel like painting myself into any corners just now...


