/*
 * ANOS Stage 2 (ANLOAD)
 * Link Script
 *
 * (c)2023 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: MIT)
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

MEMORY {
    STAGE2  : org = 0x00009c00,  l = 0x00006400     /* 25KB for Stage 2 (up to segment boundary) */
}

SECTIONS
{
  .text :
  {
    _code = .;
    *(.text*)
    *(.rodata*)
    _code_end = .;
    _data_start = .;
    *(.data*)
    . = ALIGN(4);
    _data_end = .;
  } > STAGE2

  _end = .;
}
