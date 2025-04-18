/*
 * ANOS kernel (STAGE3)
 * Link Script
 *
 * (c)2023 Ross Bamford & Contributors
 * See LICENSE.md (spoiler: GPLv2)
 */

ENTRY(_start_limine)

MEMORY {
    CODE    : org = 0xFFFFFFFF80130000,  l = 0x000e0000     /* 896KiB for Stage 3 code */
    DATA    : org = 0xFFFFFFFF80110000,  l = 0x00020000     /* 64KiB for bss, stack goes below this */
}

SECTIONS
{
  .bss (NOLOAD) :
  {
   _kernel_vma_start = .;
   _bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
    _stack_start = .;
    . += 0x4000;
    _stack_end = .;
    _bss_end = .;
  } > DATA AT > CODE
  
  .text :
  {
    _code = .;
    *(.text.init)
    . = ALIGN(16);
    *(.text*)
    . = ALIGN(16);
    PROVIDE(_global_pointer = .);
    *(.rodata*)
    _code_end = .;
    _data_start = .;
    . = ALIGN(16);
    *(.data*)
    . = ALIGN(16);
    _data_end = .;
  } > CODE

  .system :
  {
    . = ALIGN(4096);
    _system_bin_start = .;
    *(.system_bin*)
    . = ALIGN(4096);
    _system_bin_end = .;
  } > CODE

  .realmode :
  {
    . = ALIGN(4096);
    _realmode_bin_start = .;
    *(.realmode_bin*)
    . = ALIGN(4096);
    _realmode_bin_end = .;
  } > CODE

  _kernel_vma_end = .;
}
