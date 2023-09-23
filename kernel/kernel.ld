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
    CODE    : org = 0xFFFFFFFF80120000,  l = 0x000e0000     /* 896KiB for Stage 3 code */
    DATA    : org = 0xFFFFFFFF80110000,  l = 0x00010000     /* 64KiB for bss, stack goes below this */
}

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
    _data_start = .;
    . = ALIGN(16);
    *(.data*)
    . = ALIGN(16);
    _data_end = .;
  } > CODE

  .bss :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
    _bss_end = .;
  } > DATA AT > CODE

  _end = .;
}
