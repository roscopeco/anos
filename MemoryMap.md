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
| `0x0000000000009c00` | `0x000000000000ffff` | Stage 2 load area (max 25k, doesn't use it all)              |
| `------------------` | `0x000000000009bfff` | Stage 2 Protected stack (probably pointless moving it here..)|
| `0x000000000009c000` | `0x000000000009cfff` | Inital PML4 set up in stage2 init_pagetables.asm             |
| `0x000000000009d000` | `0x000000000009dfff` | Inital PDPT set up in stage2 init_pagetables.asm             |
| `0x000000000009e000` | `0x000000000009efff` | Inital PD set up in stage2 init_pagetables.asm               |
| `0x000000000009f000` | `0x000000000009ffff` | Inital PT set up in stage2 init_pagetables.asm               |

### Kernel Physical Mem (after stage2 is done)

| Start                | End                  | Use                                                          |
|----------------------|----------------------|--------------------------------------------------------------|
| `0x0000000000008400` | `0x0000000000009bff` | BIOS E820h memory map (passed to kernel by stage2)           |
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
* `0xFFFFFFFF80000000` -> `0xFFFFFFFF80200000` : First 2MiB of top (or negative) 2GiB mapped to first 2MiB phys

This mapping lets the long mode setup keep running as it was (in the 
identity-mapped space), up until it jumps directly to the kernel at it's
(virtual) load address (in the kernel space mapping).

This is all fun and games, but will almost certainly be a problem later 
(i.e. the kernel being stuck at <1MiB size and located at the ~1MiB mark in physical 
RAM - I'm thinking DMA or whatever will probably want that area...)

Once that happens it'll make sense to make things more robust, and maybe 
just load the kernel somewhere above 16MiB physical and map accordingly
(Kernel space can still map broadly the same - it might be useful to have
the first 16MiB mapped at the start of kernel space 🤔).

_Maybe_ I'd check we actually have >16MiB first, but are there even any 
x86_64s that have less RAM than that? 🤷