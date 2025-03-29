ARCH?=x86_64
TARGET_TRIPLE?=$(ARCH)-elf-anos

ASM?=nasm
XLD?=$(TARGET_TRIPLE)-ld
XOBJCOPY?=$(TARGET_TRIPLE)-objcopy
XOBJDUMP?=$(TARGET_TRIPLE)-objdump
XCC?=$(TARGET_TRIPLE)-gcc
XAR?=$(TARGET_TRIPLE)-ar
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-mno-mmx -mno-sse -mno-sse2 											\
		-fno-asynchronous-unwind-tables 										\
		-g																		\
		-O3																		\

SERVERS_ROOT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR=$(abspath $(SERVERS_ROOT_DIR)/..)
THIS_DIR=$(SERVERS_ROOT_DIR)/$(SERVER_NAME)

# Servers load at 0x0000000001000000 (16MiB)
#
ENTRYPOINT_ADDR?=0x01000000

BINARY_NAME=$(BINARY).bin
BINARY_UC=$(shell echo '$(BINARY)' | tr '[:lower:]' '[:upper:]')
ALL_TARGETS=$(BINARY_NAME)
CLEAN_ARTIFACTS=$(THIS_DIR)/*.dis $(THIS_DIR)/*.elf $(THIS_DIR)/*.o $(THIS_DIR)/$(BINARY_NAME)
SHORT_HASH?=`git rev-parse --short HEAD`

CDEFS=-D$(BINARY_UC)_BUILD

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
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) $(CFLAGS) -c -o $@ $<

$(BINARY).elf: $(BINARY_OBJS)
	$(XCC) -o $@ $^
	chmod a-x $@

$(BINARY).dis: $(BINARY).elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

$(BINARY_NAME): $(BINARY).elf $(BINARY).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@
