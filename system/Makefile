ARCH?=x86_64
TARGET_TRIPLE?=$(ARCH)-elf-anos

ASM?=nasm
XLD?=$(TARGET_TRIPLE)-gcc
XOBJCOPY?=$(TARGET_TRIPLE)-objcopy
XOBJDUMP?=$(TARGET_TRIPLE)-objdump
XSTRIP?=$(TARGET_TRIPLE)-strip
XCC?=$(TARGET_TRIPLE)-gcc
XAR?=$(TARGET_TRIPLE)-ar
XRANLIB?=$(TARGET_TRIPLE)-ranlib
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-fno-asynchronous-unwind-tables 										\
		-g																		\
		-O$(OPTIMIZE)

# The following C defines are recognised by system and enable various things
#
#	DEBUG_INIT_RAMFS		Dump info about the initial RAMFS loaded by system
#	DEBUG_SYS_IPC			Debug IPC messages received by SYSTEM
#	DEBUG_SERVER_LOADER		Debug the initial server program loader
#	DEBUG_FS_INIT			Debug filesystem init
#
#	TEST_THREAD_KILL		Starts a sacrificial thread to test kill syscall
#	TEST_BEEP_BOOP			Start the Beep Boop test server process
#
CDEFS=

# We don't really have a RISC-V userspace yet, so always start the beep-boop test on that platform for now...
ifeq ($(ARCH),riscv64)
CDEFS+=-DTEST_BEEP_BOOP
endif

THIS_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR=$(abspath $(THIS_DIR)/..)

SYSTEM?=system
ARCH_X86_64_REALMODE?=realmode

STAGE3?=kernel
STAGE3_DIR?=$(STAGE3)
SYSTEM_ELF?=$(SYSTEM).elf
SYSTEM_BIN?=$(SYSTEM).img

JANSSON_VER?=2.13
JANSSON_DIR?=jansson-$(JANSSON_VER)
JANSSON_SRC_DIR?=$(JANSSON_DIR)/src
JANSSON_LIB_DIR?=$(JANSSON_DIR)/src/.libs
JANSSON_LIB?=$(JANSSON_LIB_DIR)/libjansson.a

# System loads at 0x0000000001000000 (16MiB)
#
SYSTEM_ADDR?=0x01000000

SYSTEM_OBJS=main.o																\
			ramfs.o																\
			path.o																\
			loader.o															\
			elf.o																\
			arch/$(ARCH)/trampolines.o											\
			process.o															\
			$(JANSSON_LIB)														\
			system_ramfs_linkable.o

ALL_TARGETS=$(SYSTEM_BIN)

CLEAN_ARTIFACTS=*.dis *.elf *.o arch/x86_64/*.o arch/riscv64/*.o $(SYSTEM_BIN)

SHORT_HASH?=`git rev-parse --short HEAD`

.PHONY: all build clean test $(JANSSON_LIB)

all: build

build: $(ALL_TARGETS)

clean:
	rm -rf $(CLEAN_ARTIFACTS)
	$(MAKE) -C $(JANSSON_DIR) clean

ifeq ($(ARCH),x86_64)
%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	$(CDEFS)																	\
	$(ASFLAGS) 																	\
	-o $@ $<
else
ifeq ($(ARCH),riscv64)
%.o: %.S
	$(ASM) 																		\
	-x assembler-with-cpp														\
	-DVERSTR=$(SHORT_HASH) 														\
	$(STAGE3_INC)																\
	$(CFLAGS)																	\
	$(CDEFS)																	\
	-c -o $@ $<
endif
endif

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) -Iinclude -I$(JANSSON_SRC_DIR) $(CFLAGS) -c -o $@ $<

$(SYSTEM_ELF): $(SYSTEM_OBJS)
	$(XLD) -T $(SYSTEM).ld -o $@ $^
	chmod a-x $@
	cp $@ $(patsubst %.elf,%_debug.elf,$@)
	$(XSTRIP) $@

$(SYSTEM_BIN): $(SYSTEM_ELF) $(SYSTEM).dis
	$(XOBJCOPY) -O binary $< $@
	chmod a-x $@

$(ROOT_DIR)/servers/system.ramfs: $(ROOT_DIR)/servers
	$(MAKE) -C $< system_ramfs

ifeq ($(ARCH),x86_64)
$(SYSTEM).dis: $(SYSTEM_ELF)
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

system_ramfs_linkable.o: $(ROOT_DIR)/servers/system.ramfs
	$(XOBJCOPY) -I binary --rename-section .data=.system_ramfs_bin -O elf64-x86-64 --binary-architecture i386:x86-64 $< $@
else
ifeq ($(ARCH),riscv64)
$(SYSTEM).dis: $(SYSTEM_ELF)
	$(XOBJDUMP) -D -mriscv -Maddr32,data32 $< > $@

system_ramfs_linkable.o: $(ROOT_DIR)/servers/system.ramfs
	$(XOBJCOPY) -I binary --rename-section .data=.system_ramfs_bin -O elf64-littleriscv --binary-architecture riscv:rv64 $< $@
else
$(error Need an architecture-specific setup for objcopy:ramfs_linkable in system/Makefile)
endif
endif

$(JANSSON_LIB):
	cd $(JANSSON_DIR);						\
	sh configure --host=$(ARCH)-elf-anos;	\
	make
