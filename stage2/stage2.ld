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
    STAGE1  : org = 0x00007c00,  l = 0x00000800     /* Use 2KB Stage 1's 8KB memory for BSS etc */
}

SECTIONS
{
  .text :
  {
    _code = .;
    *(.text*)
    *(.rodata*)
    _code_end = .;
  } > STAGE2

  .data :
  {
    _data_start = .;
    *(.data*)
    . = ALIGN(4);
    _data_end = .;
  } > STAGE1 AT > STAGE2

  .bss :
  {
    _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > STAGE1 AT > STAGE2

  _end = .;
}
