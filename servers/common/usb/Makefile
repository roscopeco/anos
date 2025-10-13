ARCH?=x86_64
TARGET_TRIPLE?=$(ARCH)-elf-anos

ASM?=nasm
XLD?=$(TARGET_TRIPLE)-ld
XOBJCOPY?=$(TARGET_TRIPLE)-objcopy
XOBJDUMP?=$(TARGET_TRIPLE)-objdump
STRIP?=$(TARGET_TRIPLE)-strip
XCC?=$(TARGET_TRIPLE)-gcc
XAR?=$(TARGET_TRIPLE)-ar
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-fno-asynchronous-unwind-tables 										\
		-g																		\
		-O$(OPTIMIZE)

SERVERS_ROOT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR=$(abspath $(SERVERS_ROOT_DIR)/..)
THIS_DIR=$(SERVERS_ROOT_DIR)/$(SERVER_NAME)

USB_COMMON_NAME=usbcore
LIBRARY=$(USB_COMMON_NAME)
LIBRARY_OBJS=usb_core.o

LIBRARY_NAME=$(BINARY).a
LIBRARY_UC=$(shell echo '$(BINARY)' | tr '[:lower:]' '[:upper:]')
ALL_TARGETS=$(LIBRARY_NAME)
CLEAN_ARTIFACTS=$(THIS_DIR)/*.dis $(THIS_DIR)/*.elf $(THIS_DIR)/*.o $(THIS_DIR)/$(LIBRARY_NAME)
SHORT_HASH?=`git rev-parse --short HEAD`

CDEFS+=-D$(LIBRARY_UC)_BUILD

ifeq ($(CONSERVATIVE_BUILD),true)
CFLAGS+=-DCONSERVATIVE_BUILD
ifeq ($(CONSERVATIVE_UBSAN),true)
CFLAGS+=-fsanitize=undefined
LIBRARY_OBJS+=../common/ubsan.o
endif
endif

# USB core library build
CDEFS=

.PHONY: all build clean test

all: build

build: $(ALL_TARGETS)

%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	$(ASFLAGS) 																	\
	-o $@ $<

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) $(CFLAGS) -c -o $@ $<				\
	-I$(THIS_DIR)/include

$(LIBRARY_NAME): $(LIBRARY_OBJS)
	$(XAR) rcs lib$(USB_COMMON_NAME).a $(LIBRARY_OBJS)
	
clean:
	rm -f *.o *.a