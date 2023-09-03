ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
QEMU?=qemu-system-x86_64
XCC?=x86_64-elf-gcc
BOCHS?=bochs
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -g

SHORT_HASH?=`git rev-parse --short HEAD`

# Base addresses; Stage 1
STAGE_1_ADDR=0x7c00

# Stage 2 at this address leaves 8K for FAT (16 sectors) after stage1
# Works for floppy, probably won't for anything bigger...
STAGE_2_ADDR=0x9c00

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf *.dis *.elf *.o stage1 stage2 floppy.img

%.o: %.asm
	$(ASM) -DVERSTR=$(SHORT_HASH) -DSTAGE_2_ADDR=$(STAGE_2_ADDR) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(XCC) $(CFLAGS) -c -o $@ $<

stage1.dis: stage1.elf
	$(XOBJDUMP) -D -mi386 -Maddr16,data16 $< > $@

stage1.elf: stage1.o
	$(XLD) -Ttext=$(STAGE_1_ADDR) -o $@ $^

stage2.dis: stage2.elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

stage2.elf: stage2.o stage2_ctest.o
	$(XLD) -Ttext=$(STAGE_2_ADDR) -o $@ $^

stage1: stage1.elf stage1.dis
	$(XOBJCOPY) -O binary $< $@

stage2: stage2.elf stage2.dis
	$(XOBJCOPY) -O binary $< $@

floppy.img: stage1 stage2
	dd of=$@ if=/dev/zero bs=1440k count=1
	mformat -f 1440 -B stage1 -v ANOSDISK001 -k -i $@ ::
	mcopy -i $@ stage2.asm ::stage2.asm
	mcopy -i $@ stage2 ::stage2.bin

qemu: floppy.img
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac

debug-qemu-start: floppy.img
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac -gdb tcp::9666 -S &

debug-qemu: debug-qemu-start
	gdb

bochs: floppy.img bochsrc
	$(BOCHS)

