ASM?=nasm
XLD?=x86_64-elf-ld
QEMU?=qemu-system-x86_64
BOCHS?=bochs

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf *.o stage1 floppy.img

%.o : %.asm
	$(ASM) -f elf64 -o $@ $<

stage1: stage1.o
	$(XLD) --oformat binary -Ttext=0x7c00 -o $@ $^ 

floppy.img: stage1
	cp $< $@
	truncate -s 1440k $@

qemu: floppy.img
	$(QEMU) -drive file=$<,format=raw,index=0,media=disk

bochs: floppy.img bochsrc
	$(BOCHS)

