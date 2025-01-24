ASM?=nasm
XLD?=x86_64-elf-ld
XOBJCOPY?=x86_64-elf-objcopy
XOBJDUMP?=x86_64-elf-objdump
QEMU?=qemu-system-x86_64
XCC?=x86_64-elf-gcc
BOCHS?=bochs
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wpedantic -std=c23										\
		-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 				\
		-fno-asynchronous-unwind-tables 										\
		-mcmodel=kernel															\
		-O3																		\
		-g

# Setup host stuff for tests etc.
HOST_ARCH?=$(shell uname -p)
HOST_OS=$(shell uname)
ifeq ($(HOST_OS),Darwin)
HOST_OBJFORMAT=macho64
else
HOST_OBJFORMAT=elf64
endif

# The following C defines are recognised by stage3 and enable various things
#
#   CONSERVATIVE_BUILD	Will build a (slow) kernel with various invariant checks
#
#   DEBUG_VMM 			Enable debugging of the VMM
#	VERY_NOISY_VMM		Enable *lots* of debugging in the VMM (requires DEBUG_VMM)
#	DEBUG_PAGE_FAULT	Enable debugging in page fault handler
#	DEBUG_ACPI			Enable debugging in ACPI mapper / parser
#	DEBUG_MADT			Enable debug dump of the Multiple APIC Descriptor Table at boot
#	VERY_NOISY_ACPI		Enable *lots* of debugging in the ACPI (requires DEBUG_ACPI)
#   DEBUG_LAPIC_INIT	Enable debugging of LAPIC initialisation
#	DEBUG_PCI_ENUM		Enable debugging of PCI enumeration
#	VERY_NOISY_PCI_ENUM	Enable *lots* of debugging in the PCI enum (requires DEBUG_PCI_ENUM)
#	DEBUG_HPET			Enable debugging of the HPET initialisation
#	DEBUG_SLEEP			Enable debugging of the sleep (and eventually yield etc) syscall(s)
#
# These ones enable some specific feature tests
#
#	DEBUG_FORCE_HANDLED_PAGE_FAULT		Force a handled page-fault at boot
#	DEBUG_FORCE_UNHANDLED_PAGE_FAULT	Force an unhandled page-fault at boot
#   DEBUG_TASK_SWITCH					Dump debug info when switching tasks
#	DEBUG_NO_START_SYSTEM				Don't start the user-mode supervisor
#
# Additionally:
#
#	UNIT_TESTS			Enables stubs and mocks used in unit tests (don't use unless building tests!)
#
CDEFS=

SHORT_HASH?=`git rev-parse --short HEAD`

STAGE1?=stage1
STAGE2?=stage2
STAGE3?=kernel
SYSTEM?=system
LIBANOS?=libanos
STAGE1_DIR?=$(STAGE1)
STAGE2_DIR?=$(STAGE2)
STAGE3_DIR?=$(STAGE3)
SYSTEM_DIR?=$(SYSTEM)
LIBANOS_DIR?=$(LIBANOS)
STAGE1_BIN=$(STAGE1).bin
STAGE2_BIN=$(STAGE2).bin
STAGE3_BIN=$(STAGE3).bin
SYSTEM_BIN=$(SYSTEM).bin
LIBANOS_ARCHIVE=$(LIBANOS).a

export LIBANOS_DIR
export LIBANOS_ARCHIVE

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
			$(STAGE3_DIR)/printdec.o											\
			$(STAGE3_DIR)/machine.o												\
			$(STAGE3_DIR)/machine_asm.o											\
			$(STAGE3_DIR)/pic.o													\
			$(STAGE3_DIR)/interrupts.o											\
			$(STAGE3_DIR)/isr_handlers.o										\
			$(STAGE3_DIR)/isr_dispatch.o										\
			$(STAGE3_DIR)/init_interrupts.o										\
			$(STAGE3_DIR)/pagefault.o											\
			$(STAGE3_DIR)/structs/list.o										\
			$(STAGE3_DIR)/init_pagetables.o										\
			$(STAGE3_DIR)/pmm/sys_asm.o											\
			$(STAGE3_DIR)/pmm/pagealloc.o										\
			$(STAGE3_DIR)/vmm/vmmapper.o										\
			$(STAGE3_DIR)/fba/alloc.o											\
			$(STAGE3_DIR)/slab/alloc.o											\
			$(STAGE3_DIR)/acpitables.o											\
			$(STAGE3_DIR)/gdt.o													\
			$(STAGE3_DIR)/general_protection_fault.o							\
			$(STAGE3_DIR)/timer_isr.o											\
			$(STAGE3_DIR)/kdrivers/drivers.o									\
			$(STAGE3_DIR)/kdrivers/local_apic.o									\
			$(STAGE3_DIR)/kdrivers/hpet.o										\
			$(STAGE3_DIR)/pci/bus.o												\
			$(STAGE3_DIR)/pci/enumerate.o										\
			$(STAGE3_DIR)/spinlock.o											\
			$(STAGE3_DIR)/init_syscalls.o										\
			$(STAGE3_DIR)/syscalls.o											\
			$(STAGE3_DIR)/task.o												\
			$(STAGE3_DIR)/task_switch.o											\
			$(STAGE3_DIR)/task_user_entrypoint.o								\
			$(STAGE3_DIR)/sched/lock.o											\
			$(STAGE3_DIR)/sched/prr.o											\
			$(STAGE3_DIR)/structs/pq.o											\
			$(STAGE3_DIR)/sleep.o												\
			$(STAGE3_DIR)/cpuid.o												\
			$(SYSTEM)_linkable.o
			
ALL_TARGETS=floppy.img

FLOPPY_DEPENDENCIES=$(STAGE1_DIR)/$(STAGE1_BIN) 								\
					$(STAGE2_DIR)/$(STAGE2_BIN) 								\
					$(STAGE3_DIR)/$(STAGE3_BIN)

CLEAN_ARTIFACTS=$(STAGE1_DIR)/*.dis $(STAGE1_DIR)/*.elf $(STAGE1_DIR)/*.o 		\
	       		$(STAGE2_DIR)/*.dis $(STAGE2_DIR)/*.elf $(STAGE2_DIR)/*.o 		\
	       		$(STAGE3_DIR)/*.dis $(STAGE3_DIR)/*.elf $(STAGE3_DIR)/*.o 		\
	       		$(STAGE3_DIR)/pmm/*.o $(STAGE3_DIR)/vmm/*.o				 		\
				$(STAGE3_DIR)/kdrivers/*.o $(STAGE3_DIR)/pci/*.o				\
				$(STAGE3_DIR)/fba/*.o $(STAGE3_DIR)/slab/*.o					\
				$(STAGE3_DIR)/structs/*.o $(STAGE3_DIR)/sched/*.o				\
		   		$(STAGE1_DIR)/$(STAGE1_BIN) $(STAGE2_DIR)/$(STAGE2_BIN) 		\
		   		$(STAGE3_DIR)/$(STAGE3_BIN) 									\
				$(SYSTEM)_linkable.o											\
		   		$(FLOPPY_IMG)

.PHONY: all build clean qemu bochs test

all: build test

build: $(ALL_TARGETS)

clean:
	rm -rf $(CLEAN_ARTIFACTS)
	$(MAKE) -C libanos clean
	$(MAKE) -C system clean

include kernel/tests/include.mk

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

# ############# Libanos ##############
$(LIBANOS_DIR)/$(LIBANOS_ARCHIVE): $(LIBANOS_DIR)/Makefile
	$(MAKE) -C $(LIBANOS_DIR)

# ############# System  ##############
$(SYSTEM_DIR)/$(SYSTEM_BIN): $(SYSTEM_DIR)/Makefile $(LIBANOS_DIR)/$(LIBANOS_ARCHIVE)
	$(MAKE) -C $(SYSTEM_DIR)

$(SYSTEM)_linkable.o: $(SYSTEM_DIR)/$(SYSTEM_BIN)
	$(XOBJCOPY) -I binary --rename-section .data=.system_bin -O elf64-x86-64 --binary-architecture i386:x86-64 $< $@

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

QEMU_OPTS=-smp cpus=2 -drive file=$<,if=floppy,format=raw,index=0,media=disk -boot order=ac -M q35 -device ioh3420,bus=pcie.0,id=pcie.1,addr=1e -device qemu-xhci,bus=pcie.1 -monitor stdio
QEMU_DEBUG_OPTS=$(QEMU_OPTS) -gdb tcp::9666 -S

qemu: $(FLOPPY_IMG)
	$(QEMU) $(QEMU_OPTS)

debug-qemu-start: $(FLOPPY_IMG)
	$(QEMU) $(QEMU_DEBUG_OPTS)

debug-qemu-start-terminal: $(FLOPPY_IMG)
	$(QEMU) $(QEMU_DEBUG_OPTS) &

debug-qemu: debug-qemu-start-terminal
	gdb -ex 'target remote localhost:9666'

bochs: floppy.img bochsrc
	$(BOCHS)
