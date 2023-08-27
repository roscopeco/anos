ASM?=nasm

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf *.o stage1 floppy.img

% : %.asm
	$(ASM) -f bin -o $@ $<

floppy.img: stage1
	cp $< $@
	truncate -s 1440k $@

qemu: floppy.img
	qemu-system-i386 -drive file=$<,format=raw,index=0,media=disk

bochs: floppy.img bochsrc
	bochs

