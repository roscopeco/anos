/*
 * ANOS system (usermode manager)
 * Link Script
 *
 * (c)2023 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: GPLv2)
 */

ENTRY(_start)

MEMORY {
    CODE    : org = 0x0000000001000000,  l = 0x0010000000     /* 256MiB for System code */
    DATA    : org = 0x0000000040000000,  l = 0x0040000000     /* 1GiB for bss/data, stack goes below this */
}

PROVIDE(__user_stack_top = 0x0000000080000000);

SECTIONS
{
  .text :
  {
    _code_start = .;
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
    . = ALIGN(4096);
    _system_ramfs_start = .;
    *(.system_ramfs_bin*)
    _system_ramfs_end = .;
    . = ALIGN(4096);
  } > CODE

  .bss (NOLOAD) :
  {
    _bss_start = .;
    *(.bss*)
    . = ALIGN(16);
    *(COMMON)
    . = ALIGN(16);
    _bss_end = .;
  } > DATA

  _end = .;
}
