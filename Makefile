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
		-mcmodel=kernel															\
		-g
HOST_ARCH?=macho64

# The following C defines are recognised by stage3 and enable various things
#
#   DEBUG_VMM 			Enable debugging of the VMM
#	VERY_NOISY_VMM		Enable *lots* of debugging in the VMM (requires DEBUG_VMM)
#	DEBUG_PAGE_FAULT	Enable debugging in page fault handler
#	DEBUG_ACPI		Enable debugging in ACPI mapper / parser
#	VERY_NOISY_ACPI		Enable *lots* of debugging in the ACPI (requires DEBUG_ACPI)
#
# These ones enable some specific feature tests
#
#	DEBUG_FORCE_HANDLED_PAGE_FAULT
#	DEBUG_FORCE_UNHANDLED_PAGE_FAULT
#
# Additionally:
#
#	UNIT_TESTS			Enables stubs and mocks used in unit tests (don't use unless building tests!)
#
CDEFS=-DDEBUG_MADT

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

STAGE3_INC=$(STAGE3)/include

# Base addresses; Stage 1
STAGE_1_ADDR?=0x7c00

# Stage 2 at this address leaves 8K for FAT (16 sectors) after stage1, 
# before (potentially - at least on bochs and qemu) EBDA.
#
# Works for floppy, probably won't for anything bigger...
#
# It also means stage 2 is limited to 23KiB, which is probably fine 😅
#
STAGE_2_ADDR?=0xa400

# Stage 3 loads at 0x00120000 (just after 1MiB, leaving a bit for BSS etc
# at 1MiB), but runs as 0xFFFFFFFF80120000 (in the top / negative 2GB).
#
# Initial page tables in stage 2 map both to the same physical memory.
STAGE_3_LO_ADDR?=0x00120000
STAGE_3_HI_ADDR?=0xFFFFFFFF80120000

FLOPPY_IMG?=floppy.img

STAGE2_OBJS=$(STAGE2_DIR)/$(STAGE2).o 											\
			$(STAGE2_DIR)/prints.o 												\
			$(STAGE2_DIR)/memorymap.o 											\
			$(STAGE2_DIR)/a20.o 												\
			$(STAGE2_DIR)/modern.o 												\
			$(STAGE2_DIR)/fat.o 												\
			$(STAGE2_DIR)/init_pagetables.o
			
STAGE3_OBJS=$(STAGE3_DIR)/init.o 												\
			$(STAGE3_DIR)/entrypoint.o											\
			$(STAGE3_DIR)/debugprint.o											\
			$(STAGE3_DIR)/printhex.o											\
			$(STAGE3_DIR)/machine.o												\
			$(STAGE3_DIR)/pic.o													\
			$(STAGE3_DIR)/interrupts.o											\
			$(STAGE3_DIR)/isr_handlers.o										\
			$(STAGE3_DIR)/isr_dispatch.o										\
			$(STAGE3_DIR)/init_interrupts.o										\
			$(STAGE3_DIR)/pagefault.o											\
			$(STAGE3_DIR)/init_pagetables.o										\
			$(STAGE3_DIR)/pmm/pagealloc.o										\
			$(STAGE3_DIR)/vmm/vmmapper.o										\
			$(STAGE3_DIR)/acpitables.o											\
			$(STAGE3_DIR)/kdrivers/drivers.o									\
			$(STAGE3_DIR)/kdrivers/local_apic.o
			
			
ALL_TARGETS=floppy.img

FLOPPY_DEPENDENCIES=$(STAGE1_DIR)/$(STAGE1_BIN) 								\
					$(STAGE2_DIR)/$(STAGE2_BIN) 								\
					$(STAGE3_DIR)/$(STAGE3_BIN)

CLEAN_ARTIFACTS=$(STAGE1_DIR)/*.dis $(STAGE1_DIR)/*.elf $(STAGE1_DIR)/*.o 		\
	       		$(STAGE2_DIR)/*.dis $(STAGE2_DIR)/*.elf $(STAGE2_DIR)/*.o 		\
	       		$(STAGE3_DIR)/*.dis $(STAGE3_DIR)/*.elf $(STAGE3_DIR)/*.o 		\
	       		$(STAGE3_DIR)/pmm/*.o $(STAGE3_DIR)/vmm/*.o				 		\
		   		$(STAGE1_DIR)/$(STAGE1_BIN) $(STAGE2_DIR)/$(STAGE2_BIN) 		\
		   		$(STAGE3_DIR)/$(STAGE3_BIN) 									\
		   		$(FLOPPY_IMG)

HOST_ARCH=$(shell uname -p)

.PHONY: all clean qemu bochs test

all: $(ALL_TARGETS)

ifeq ($(HOST_ARCH),arm)
$(info WARN: Host architecture is ARM, tests will not be included...)
test:
	@echo Host architecture is ARM, tests requested but they were skipped
else
include tests/include.mk
endif

clean:
	rm -rf $(CLEAN_ARTIFACTS)

%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	-DSTAGE_2_ADDR=$(STAGE_2_ADDR)												\
	-DSTAGE_3_LO_ADDR=$(STAGE_3_LO_ADDR) -DSTAGE_3_HI_ADDR=$(STAGE_3_HI_ADDR)	\
	$(ASFLAGS) 																	\
	-o $@ $<

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) -I$(STAGE3_INC) $(CFLAGS) -c -o $@ $<


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
$(FLOPPY_IMG): $(FLOPPY_DEPENDENCIES)
	dd of=$@ if=/dev/zero bs=1440k count=1
	mformat -f 1440 -B $(STAGE1_DIR)/$(STAGE1_BIN) -v ANOSDISK001 -k -i $@ ::
	mcopy -i $@ $(STAGE2_DIR)/$(STAGE2_BIN) ::$(STAGE2_BIN)
	mcopy -i $@ $(STAGE3_DIR)/$(STAGE3_BIN) ::$(STAGE3_BIN)

qemu: $(FLOPPY_IMG)
	$(QEMU) -smp cpus=2 -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac

QEMU_OPTS=-drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac -gdb tcp::9666 -S

debug-qemu-start: $(FLOPPY_IMG)
	$(QEMU) $(QEMU_OPTS)

debug-qemu-start-terminal: $(FLOPPY_IMG)
	$(QEMU) $(QEMU_OPTS) &

debug-qemu: debug-qemu-start-terminal
	gdb -ex 'target remote localhost:9666'

bochs: floppy.img bochsrc
	$(BOCHS)
