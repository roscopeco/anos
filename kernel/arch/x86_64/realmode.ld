/*
 * ANOS kernel (STAGE3) - Realmode code
 * Link Script
 *
 * (c)2025 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: GPLv2)
 *
 * Yes, I'm aware this could all be linked by a single script,
 * but that was such a pain in the arse that I decided to build
 * the realmode code as a linkable blob instead (in a similar
 * way SYSTEM is handled).
 *
 * This way I don't have to worry about weirdness crossing the
 * address spaces etc, everything is self contained.
 *
 * So note to future me: if you've come here to "tidy this up"
 * you better be damn sure you have a good reason for it......
 *
 */

OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

MEMORY {
    CODE    : org = 0x00001000,  l = 0x00004000                /* 4KiB (max) for realmode code */
    DATA    : org = 0x00005000,  l = 0x00001000                /* 4KiB for bss */
    STACKS  : org = 0x00006000,  l = 0x00008000                /* 32KiB for stacks, each AP gets 2KiB bytes */
                                                               /* (based on current MAX_CPU_COUNT 16 CPUs) */
}

PROVIDE(STACKS_BASE = ORIGIN(STACKS));
PROVIDE(STACK_SHIFT = 11);
PROVIDE(STACK_START_OFS = 0x7f8);
PROVIDE(KERNEL_BASE = 0xffffffff80000000);

SECTIONS
{
  .text :
  {
    _code = .;
    *(.text.init)
    . = ALIGN(16);
    *(.text*)
    . = ALIGN(16);
    *(.rodata*)
    _code_end = .;
  } > CODE

  .data :
  {
    _data_start = .;
    . = ALIGN(16);
    *(.data*)
    . = ALIGN(16);
    _data_end = .;
  } > DATA AT > CODE

  .bss (NOLOAD) :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
    _bss_end = .;
  } > DATA AT > CODE

  _end = .;
}
