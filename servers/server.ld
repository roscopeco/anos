/*
 * ANOS system (usermode manager)
 * Link Script
 *
 * (c)2023 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: GPLv2)
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

MEMORY {
    CODE    : org = 0x0000000002000000,  l = 0x0040000000     /* 1GiB for server code */
    DATA    : org = 0x0000000080000000,  l = 0x4000000000     /* 256GiB for bss, stack goes below this */
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
  } > CODE
  
  .bss (NOLOAD) :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
    _bss_end = .;
  } > DATA AT > CODE

  .data : 
  {
    _data_start = .;
    . = ALIGN(16);
    *(.data*)
    . = ALIGN(16);
    _data_end = .;
  } > DATA

  _end = .;
}
