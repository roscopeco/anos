ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
QEMU?=qemu-system-x86_64
BOCHS?=bochs

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf *.dis *.elf *.o stage1 floppy.img

%.o: %.asm
	$(ASM) -f elf64 -o $@ $<

%.dis: %.elf
	$(XOBJDUMP) -D $< > $@

stage1.elf: stage1.o
	$(XLD) -Ttext=0x7c00 -o $@ $^ 

stage1: stage1.elf stage1.dis
	$(XOBJCOPY) -O binary $< $@

floppy.img: stage1
	cp $< $@
	truncate -s 1440k $@

qemu: floppy.img
	$(QEMU) -drive file=$<,format=raw,index=0,media=disk

bochs: floppy.img bochsrc
	$(BOCHS)

