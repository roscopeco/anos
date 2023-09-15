/*
 * ANOS Stage 3 (ANINIT)
 * Link Script
 *
 * (c)2023 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: MIT)
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

MEMORY {
    CODE    : org = 0x00120000,  l = 0x00100000     /* 1MiB for Stage 3 code */
    DATA    : org = 0x00020000,  l = 0x00010000     /* 128KiB for bss */
}

SECTIONS
{
  .text :
  {
    _code = .;
    *(.text.init)
    *(.text*)
    *(.rodata*)
    _code_end = .;
    _data_start = .;
    *(.data*)
    . = ALIGN(4);
    _data_end = .;
  } > CODE

  .bss :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > DATA AT > CODE

  _end = .;
}
