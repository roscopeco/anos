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

STAGE1?=stage1
STAGE2?=stage2
STAGE1_DIR?=$(STAGE1)
STAGE2_DIR?=$(STAGE2)
STAGE1_BIN=$(STAGE1).bin
STAGE2_BIN=$(STAGE2).bin

# Base addresses; Stage 1
STAGE_1_ADDR?=0x7c00

# Stage 2 at this address leaves 8K for FAT (16 sectors) after stage1
# Works for floppy, probably won't for anything bigger...
STAGE_2_ADDR?=0x9c00

FLOPPY_IMG?=floppy.img

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf $(STAGE1_DIR)/*.dis $(STAGE1_DIR)/*.elf $(STAGE1_DIR)/*.o 			\
	       $(STAGE2_DIR)/*.dis $(STAGE2_DIR)/*.elf $(STAGE2_DIR)/*.o 			\
		   $(STAGE1_BIN) $(STAGE2_BIN) $(FLOPPY_IMG)

%.o: %.asm
	$(ASM) -DVERSTR=$(SHORT_HASH) -DSTAGE_2_ADDR=$(STAGE_2_ADDR) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(XCC) $(CFLAGS) -c -o $@ $<

$(STAGE1_DIR)/$(STAGE1).dis: $(STAGE1_DIR)/$(STAGE1).elf
	$(XOBJDUMP) -D -mi386 -Maddr16,data16 $< > $@

$(STAGE1_DIR)/$(STAGE1).elf: $(STAGE1_DIR)/$(STAGE1).o
	$(XLD) -Ttext=$(STAGE_1_ADDR) -o $@ $^
	chmod a-x $@

$(STAGE1_DIR)/$(STAGE1_BIN): $(STAGE1_DIR)/$(STAGE1).elf $(STAGE1_DIR)/$(STAGE1).dis
	$(XOBJCOPY) -O binary $< $@
	chmod a-x $@

$(STAGE2_DIR)/$(STAGE2).dis: $(STAGE2_DIR)/$(STAGE2).elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

$(STAGE2_DIR)/$(STAGE2).elf: $(STAGE2_DIR)/$(STAGE2).o $(STAGE2_DIR)/$(STAGE2)_ctest.o
	$(XLD) -Ttext=$(STAGE_2_ADDR) -o $@ $^
	chmod a-x $@

$(STAGE2_DIR)/$(STAGE2_BIN): $(STAGE2_DIR)/$(STAGE2).elf $(STAGE2_DIR)/$(STAGE2).dis
	$(XOBJCOPY) -O binary $< $@
	chmod a-x $@

$(FLOPPY_IMG): $(STAGE1_DIR)/$(STAGE1_BIN) $(STAGE2_DIR)/$(STAGE2_BIN)
	dd of=$@ if=/dev/zero bs=1440k count=1
	mformat -f 1440 -B $(STAGE1_DIR)/$(STAGE1_BIN) -v ANOSDISK001 -k -i $@ ::
	mcopy -i $@ $(STAGE2_DIR)/$(STAGE2_BIN) ::$(STAGE2_BIN)

qemu: $(FLOPPY_IMG)
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac

debug-qemu-start: $(FLOPPY_IMG)
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac -gdb tcp::9666 -S &

debug-qemu: debug-qemu-start
	gdb

bochs: floppy.img bochsrc
	$(BOCHS)

