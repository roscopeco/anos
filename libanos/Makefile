ARCH?=x86_64
TARGET_TRIPLE?=$(ARCH)-elf-anos

ASM?=nasm
XLD?=$(TARGET_TRIPLE)-ld
XOBJCOPY?=$(TARGET_TRIPLE)-objcopy
XOBJDUMP?=$(TARGET_TRIPLE)-objdump
XCC?=$(TARGET_TRIPLE)-gcc
XAR?=$(TARGET_TRIPLE)-ar
XRANLIB?=$(TARGET_TRIPLE)-ranlib
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 				\
		-fno-asynchronous-unwind-tables 										\
		-mcmodel=large															\
		-O3																		\
		-g
ARFLAGS=

USER_INC=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))/include

SHORT_HASH?=`git rev-parse --short HEAD`

LIBBASE?=libanos
LIB_ARCHIVE=$(LIBBASE).a

LIB_ARCHIVE_OBJS=anos.o printf.o printf_putchar.o version.o sleep.o

ALL_TARGETS=$(LIB_ARCHIVE)

CLEAN_ARTIFACTS=$(LIB_ARCHIVE_OBJS) $(LIB_ARCHIVE)

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
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) -I$(USER_INC) $(CFLAGS) -c -o $@ $<

$(LIB_ARCHIVE): $(LIB_ARCHIVE_OBJS)
	$(XAR) $(ARFLAGS) rs $@ $^
	$(XRANLIB) $@
