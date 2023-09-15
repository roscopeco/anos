ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
QEMU?=qemu-system-x86_64
XCC?=x86_64-elf-gcc
BOCHS?=bochs
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic 												\
		-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 				\
		-fno-asynchronous-unwind-tables 										\
		-g

SHORT_HASH?=`git rev-parse --short HEAD`

STAGE1?=stage1
STAGE2?=stage2
STAGE3?=kernel
STAGE1_DIR?=$(STAGE1)
STAGE2_DIR?=$(STAGE2)
STAGE3_DIR?=$(STAGE3)
STAGE1_BIN=$(STAGE1).bin
STAGE2_BIN=$(STAGE2).bin
STAGE3_BIN=$(STAGE3).bin

# Base addresses; Stage 1
STAGE_1_ADDR?=0x7c00

# Stage 2 at this address leaves 8K for FAT (16 sectors) after stage1
# Works for floppy, probably won't for anything bigger...
STAGE_2_ADDR?=0x9c00

# Stage 3 loads at 0x00120000 (just after 1MiB, leaving a bit for BSS etc
# at 1MiB).
STAGE_3_ADDR?=0x00120000

FLOPPY_IMG?=floppy.img

STAGE2_OBJS=$(STAGE2_DIR)/$(STAGE2).o 											\
			$(STAGE2_DIR)/prints.o 												\
			$(STAGE2_DIR)/memorymap.o 											\
			$(STAGE2_DIR)/a20.o 												\
			$(STAGE2_DIR)/modern.o 												\
			$(STAGE2_DIR)/fat.o 												\
			$(STAGE2_DIR)/init_pagetables.o
			
STAGE3_OBJS=$(STAGE3_DIR)/init.o $(STAGE3_DIR)/entrypoint.o

.PHONY: all clean qemu bochs

all: floppy.img

clean:
	rm -rf $(STAGE1_DIR)/*.dis $(STAGE1_DIR)/*.elf $(STAGE1_DIR)/*.o 			\
	       $(STAGE2_DIR)/*.dis $(STAGE2_DIR)/*.elf $(STAGE2_DIR)/*.o 			\
	       $(STAGE3_DIR)/*.dis $(STAGE3_DIR)/*.elf $(STAGE3_DIR)/*.o 			\
		   $(STAGE2_DIR)/$(STAGE1_BIN) $(STAGE2_DIR)/$(STAGE2_BIN) 				\
		   $(STAGE3_DIR)/$(STAGE3_BIN) 											\
		   $(FLOPPY_IMG)

%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	-DSTAGE_2_ADDR=$(STAGE_2_ADDR) -DSTAGE_3_ADDR=$(STAGE_3_ADDR) 				\
	$(ASFLAGS) 																	\
	-o $@ $<

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CFLAGS) -c -o $@ $<


# ############# Stage 1 ##############
$(STAGE1_DIR)/$(STAGE1).elf: $(STAGE1_DIR)/$(STAGE1).o
	$(XLD) -Ttext=$(STAGE_1_ADDR) -o $@ $^
	chmod a-x $@

$(STAGE1_DIR)/$(STAGE1).dis: $(STAGE1_DIR)/$(STAGE1).elf
	$(XOBJDUMP) -D -mi386 -Maddr16,data16 $< > $@

$(STAGE1_DIR)/$(STAGE1_BIN): $(STAGE1_DIR)/$(STAGE1).elf $(STAGE1_DIR)/$(STAGE1).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@

# ############# Stage 2 ##############
$(STAGE2_DIR)/$(STAGE2).elf: $(STAGE2_OBJS)
	$(XLD) -T $(STAGE2_DIR)/$(STAGE2).ld -o $@ $^
	chmod a-x $@

$(STAGE2_DIR)/$(STAGE2).dis: $(STAGE2_DIR)/$(STAGE2).elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

$(STAGE2_DIR)/$(STAGE2_BIN): $(STAGE2_DIR)/$(STAGE2).elf $(STAGE2_DIR)/$(STAGE2).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@

# ############# Stage 3 ##############
$(STAGE3_DIR)/$(STAGE3).elf: $(STAGE3_OBJS)
	$(XLD) -T $(STAGE3_DIR)/$(STAGE3).ld -o $@ $^
	chmod a-x $@

$(STAGE3_DIR)/$(STAGE3).dis: $(STAGE3_DIR)/$(STAGE3).elf
	$(XOBJDUMP) -D -S -Maddr64,data64 $< > $@

$(STAGE3_DIR)/$(STAGE3_BIN): $(STAGE3_DIR)/$(STAGE3).elf $(STAGE3_DIR)/$(STAGE3).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@

# ############## Image ###############
$(FLOPPY_IMG): $(STAGE1_DIR)/$(STAGE1_BIN) $(STAGE2_DIR)/$(STAGE2_BIN) $(STAGE3_DIR)/$(STAGE3_BIN)
	dd of=$@ if=/dev/zero bs=1440k count=1
	mformat -f 1440 -B $(STAGE1_DIR)/$(STAGE1_BIN) -v ANOSDISK001 -k -i $@ ::
	mcopy -i $@ $(STAGE2_DIR)/$(STAGE2_BIN) ::$(STAGE2_BIN)
	mcopy -i $@ $(STAGE3_DIR)/$(STAGE3_BIN) ::$(STAGE3_BIN)

qemu: $(FLOPPY_IMG)
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac

debug-qemu-start: $(FLOPPY_IMG)
	$(QEMU) -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac -gdb tcp::9666 -S &

debug-qemu: debug-qemu-start
	gdb

bochs: floppy.img bochsrc
	$(BOCHS)

