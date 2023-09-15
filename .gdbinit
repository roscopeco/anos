add-symbol-file stage1/stage1.elf
add-symbol-file stage2/stage2.elf
add-symbol-file kernel/kernel.elf
target remote localhost:9666
