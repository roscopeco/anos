ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
XCC?=x86_64-elf-gcc
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 				\
		-fno-asynchronous-unwind-tables 										\
		-mcmodel=large															\
		-g																		\
		-O3																		\

# The following C defines are recognised by system and enable various things
#
#   DEBUG_INT_SYSCALLS		Use int 0x69 instead of syscall instruction
#	DEBUG_TEST_SYSCALL		Sanity-check the TEST syscall at user-mode startup
#	DEBUG_INIT_RAMFS		Dump info about the initial RAMFS loaded by system
#
CDEFS=-DDEBUG_INIT_RAMFS

THIS_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR=$(abspath $(THIS_DIR)/..)
USER_INC=$(ROOT_DIR)/$(LIBANOS_DIR)/include

# System loads at 0x0000000001000000 (16MiB)
#
SYSTEM_ADDR?=0x01000000

SYSTEM?=system
SYSTEM_BIN=$(SYSTEM).bin

SYSTEM_OBJS=start.o																\
			main.o																\
			ramfs.o																\
			system_ramfs_linkable.o												\
			$(ROOT_DIR)/$(LIBANOS_DIR)/$(LIBANOS_ARCHIVE)

ALL_TARGETS=$(SYSTEM_BIN)

CLEAN_ARTIFACTS=*.dis *.elf *.o $(SYSTEM_BIN)

SHORT_HASH?=`git rev-parse --short HEAD`

.PHONY: all build clean test

all: build

build: $(ALL_TARGETS)

clean:
	rm -rf $(CLEAN_ARTIFACTS)

%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	$(ASFLAGS) 																	\
	-o $@ $<

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) -I$(USER_INC) -Iinclude $(CFLAGS) -c -o $@ $<

$(SYSTEM).elf: $(SYSTEM_OBJS)
	$(XLD) -T $(SYSTEM).ld -o $@ $^
	chmod a-x $@

$(SYSTEM).dis: $(SYSTEM).elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

$(SYSTEM_BIN): $(SYSTEM).elf $(SYSTEM).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@

$(ROOT_DIR)/servers/system.ramfs: $(ROOT_DIR)/servers
	$(MAKE) -C $< system_ramfs

system_ramfs_linkable.o: $(ROOT_DIR)/servers/system.ramfs
	$(XOBJCOPY) -I binary --rename-section .data=.system_ramfs_bin -O elf64-x86-64 --binary-architecture i386:x86-64 $< $@

