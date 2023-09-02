ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
QEMU?=qemu-system-x86_64
BOCHS?=bochs

SHORT_HASH?=`git rev-parse --short HEAD`

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf *.dis *.elf *.o stage1 stage2 floppy.img

%.o: %.asm
	$(ASM) -D VERSTR=$(SHORT_HASH) -f elf64 -o $@ $<

stage1.dis: stage1.elf
	$(XOBJDUMP) -D -mi386 -Maddr16,data16 $< > $@

stage1.elf: stage1.o
	$(XLD) -Ttext=0x7c00 -o $@ $^ 

stage2.dis: stage2.elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

stage2.elf: stage2.o
	$(XLD) -Ttext=0x9c00 -o $@ $^ 

stage1: stage1.elf stage1.dis
	$(XOBJCOPY) -O binary $< $@

stage2: stage2.elf stage2.dis
	$(XOBJCOPY) -O binary $< $@

floppy.img: stage1 stage2
	dd of=$@ if=/dev/zero bs=1440k count=1
	mformat -f 1440 -B stage1 -v ANOS-DISK01 -k -i $@ ::
	mcopy -i $@ stage2.asm ::stage2.asm
	mcopy -i $@ stage2 ::stage2.bin

qemu: floppy.img
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk

debug-qemu: floppy.img
	echo "Start gdb: gdb -ex \"target remote localhost:9666\""
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -gdb tcp::9666 -S

bochs: floppy.img bochsrc
	$(BOCHS)

