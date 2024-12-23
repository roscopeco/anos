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
		-O3																		\
		-g

# The following C defines are recognised by stage3 and enable various things
#
#   DEBUG_INT_SYSCALLS		Use int 0x69 instead of syscall instruction
#
CDEFS=-DDEBUG_INT_SYSCALLS

# System loads at 0x0000000001000000 (16MiB)
#
SYSTEM_ADDR?=0x01000000

SYSTEM?=system
SYSTEM_BIN=$(SYSTEM).bin

SYSTEM_OBJS=start.o																\
			anos.o																\
			main.o

ALL_TARGETS=$(SYSTEM_BIN)

CLEAN_ARTIFACTS=*.dis *.elf *.o $(SYSTEM_BIN)

SHORT_HASH?=`git rev-parse --short HEAD`

.PHONY: all build clean qemu bochs test

all: build test

build: $(ALL_TARGETS)

clean:
	rm -rf $(CLEAN_ARTIFACTS)

%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	$(ASFLAGS) 																	\
	-o $@ $<

%.o: %.c
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) -I$(STAGE3_INC) $(CFLAGS) -c -o $@ $<

$(SYSTEM).elf: $(SYSTEM_OBJS)
	$(XLD) -T $(SYSTEM).ld -o $@ $^
	chmod a-x $@

$(SYSTEM).dis: $(SYSTEM).elf
	$(XOBJDUMP) -D -mi386 -Maddr32,data32 $< > $@

$(SYSTEM_BIN): $(SYSTEM).elf $(SYSTEM).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@
