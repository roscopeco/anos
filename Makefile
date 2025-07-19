# Makefile for anos.
# Copyright (c) 2025 Ross Bamford
# See LICENSE.md and related documents
#
# You can pass a few options to `make` when running this to
# influence the build / behaviour:
#
#	* ARCH=[x86_64 | riscv64]	Select target architecture
# 	* OPTIMIZE=n 				Set GCC optimization level to `n`
#
# See also the C defines further down in this file for
# more direct compile-time option settings.
#

ARCH?=x86_64

TARGET_TRIPLE?=$(ARCH)-elf-anos

ifeq ($(ARCH),x86_64)
ASM?=nasm
else
ifeq ($(ARCH),riscv64)
ASM?=$(TARGET_TRIPLE)-gcc
endif
endif

export ASM

OPTIMIZE?=3
XLD?=$(TARGET_TRIPLE)-ld
XOBJCOPY?=$(TARGET_TRIPLE)-objcopy
XOBJDUMP?=$(TARGET_TRIPLE)-objdump
QEMU?=qemu-system-$(ARCH)
XCC?=$(TARGET_TRIPLE)-gcc
BOCHS?=bochs
ASFLAGS=-f elf64 -F dwarf -g
CFLAGS=-Wall -Werror -Wno-unused-but-set-variable -Wno-unused-variable -std=c23	\
		-ffreestanding -fno-asynchronous-unwind-tables							\
		-g -O$(OPTIMIZE)														\
		-DARCH=$(ARCH) -DARCH_$(shell echo '$(ARCH)' | tr '[:lower:]' '[:upper:]')

export OPTIMIZE

ifeq ($(ARCH),x86_64)
CFLAGS+=-mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel
else
ifeq ($(ARCH),riscv64)
CFLAGS+=-mcmodel=medany -DRISCV_SV48
endif
endif

# Setup host stuff for tests etc.
HOST_ARCH?=$(shell uname -p)
HOST_OS=$(shell uname)
ifeq ($(HOST_OS),Darwin)
HOST_OBJFORMAT=macho64
else
HOST_OBJFORMAT=elf64
endif

# The following C defines are recognised by stage3 and enable various things. Some
# of them are architecture-dependent.
#
#	CONSERVATIVE_BUILD		Will build a (slow) kernel with various invariant checks
#							You should pass this to the make command if you want it.
#
#	CONSERVATIVE_PANICKY	Conservative builds will panic rather than warn
#							You should pass this to the make command if you want it.
#							(Requires CONSERVATIVE_BUILD)
#
#	DEBUG_MEMMAP			Enable debugging of the initial memory map
#	DEBUG_PMM				Enable debugging of the PMM
#	VERY_NOISY_PMM			Enable *lots* of debugging of the PMM (requires DEBUG_PMM)
#	DEBUG_VMM				Enable debugging of the VMM
#	VERY_NOISY_VMM			Enable *lots* of debugging in the VMM (requires DEBUG_VMM)
#	DEBUG_PAGE_FAULT		Enable debugging in page fault handler
#	DEBUG_ACPI				Enable debugging in ACPI mapper / parser
#	DEBUG_MADT				Enable debug dump of the Multiple APIC Descriptor Table at boot
#	VERY_NOISY_ACPI			Enable *lots* of debugging in the ACPI (requires DEBUG_ACPI)
#	DEBUG_LAPIC_INIT		Enable debugging of LAPIC initialisation
#	DEBUG_HPET				Enable debugging of the HPET initialisation
#	DEBUG_SLEEP				Enable debugging of the sleep (and eventually yield etc) syscall(s)
#	VERY_NOISY_SLEEP		Enable *lots* of debugging of sleep syscall(s)
#	DEBUG_CPU				Enable debugging of CPU information at boot
#	DEBUG_CPU_FREQ			Enable debugging of CPU frequency calibration (requires DEBUG_CPU)
#	DEBUG_SMP_STARTUP		Enable debugging of SMP AP startup
#	VERY_NOISY_SMP_STARTUP	Enable *lots* of debugging in the SMP startup routines
#	DEBUG_ADDR_SPACE		Enable debugging of address-space management
#   DEBUG_PROCESS_SYSCALLS	Enable debugging of process-related syscalls
#	DEBUG_TASK_SWITCH		Enable debugging info when switching tasks
#	VERY_NOISY_TASK_SWITCH	Enable *lots* of debugging info when switching tasks (requires DEBUG_TASK_SWITCH)
#	DEBUG_CHANNEL_IPC		Enable debugging info for IPC channels
#	DEBUG_SBI				Enable debugging of the Supervisor Binary Interface (RISC-V only)
#   DEBUG_REGION_SYSCALLS	Enable debugging of process-related syscalls
#	DEBUG_PAGEFAULT			Enable debugging of pagefault handling
#	VERY_NOISY_PAGEFAULT	Enable **lots** of debugging of pagefault handling
#	DEBUG_SYSCALL_CAPS		Enable debugging of system call capability assignments (insecure!)
#
# These ones enable some specific features
#
#	DEBUG_NO_START_SYSTEM				Don't start the user-mode supervisor
#
#	SYSCALL_SCHED_ONLY_THIS_CPU			Syscalls will only schedule things on the current CPU
#
#	DEBUG_ADDRESS_SPACE_CREATE_COPY_ALL	address_space_create will copy **all** PDPT entries,
#										not just kernel ones. This is unlikely to ever be a
#										good idea outside some very specific startup tests!
#
#	EXPERIMENTAL_SCHED_LOCK				Change the way the scheduler lock works.
#										The experimental way is simpler, but less well tested...
#
#   MAP_VIRT_SYSCALL_STATIC 			Don't lazily allocate in the anos_map_virtual syscall,
#										use the old way instead (immediately allocate pages,
#										and fail the call if we can't allocate).
#
#	ENABLE_SYSCALL_THROTTLE_RESET		Enable reset of the syscall abuse throttle after a
#										successful syscall. This should not be used in a
#										production kernel as it can allow user code to circumvent
#										the brute-force protection on syscall capabilities.
#
# These set options you might feel like configuring
#
#	USE_BIZCAT_FONT			Use BIZCAT font instead of the default (only for graphical terminal)
#
# And these will selectively disable features
#
#	NO_SMP					Disable SMP (don't spin-up any of the APs)
#	SMP_TWO_SIPI_ATTEMPTS	Try a second SIPI if an AP doesn't respond to the first (x86-only)
#	NO_USER_GS				Disable user-mode GS swap at kernel entry/exit (x86-only, debugging only)
#	NAIVE_MEMCPY			Use a naive (byte-wise only) memcpy
#	TARGET_CPU_USE_SLEEPERS	Consider the size of the sleep queue as well as run queues when selecting a target CPU
#	NO_BANNER				Disable the startup banner
#
# Additionally:
#
#	UNIT_TESTS			Enables stubs and mocks used in unit tests (don't use unless building tests!)
#
CDEFS+=-DDEBUG_CPU -DEXPERIMENTAL_SCHED_LOCK -DTARGET_CPU_USE_SLEEPERS

ifeq ($(ARCH),x86_64)
QEMU_BASEOPTS=-smp cpus=4 -cpu Haswell-v4 -m 256M -M q35 -device ioh3420,bus=pcie.0,id=pcie.1,addr=1e -device qemu-xhci,bus=pcie.1 -d int,mmu,cpu_reset,guest_errors,unimp -D qemu.log --no-reboot -monitor stdio
QEMU_UEFI_OPTS=-drive file=$(UEFI_IMG),if=ide,format=raw -drive if=pflash,format=raw,readonly=on,file=uefi/x86_64/ovmf/OVMF-pure-efi.fd -drive if=pflash,format=raw,file=uefi/x86_64/ovmf/OVMF_VARS-pure-efi.fd
else
ifeq ($(ARCH),riscv64)
QEMU_BASEOPTS=-M virt,pflash0=pflash0,pflash1=pflash1,acpi=off -m 8G -cpu rv64,sv57=on								\
    -device VGA																										\
    -device qemu-xhci																								\
    -device usb-kbd																									\
    -monitor stdio
QEMU_UEFI_OPTS=-drive file=$(UEFI_IMG),format=raw,id=hd0															\
	-blockdev node-name=pflash0,driver=file,read-only=on,filename=uefi/riscv64/edk2/RISCV_VIRT_CODE.fd				\
    -blockdev node-name=pflash1,driver=file,filename=uefi/riscv64/edk2/RISCV_VIRT_VARS.fd
endif
endif
QEMU_DEBUG_OPTS=-gdb tcp::9666 -S -monitor telnet:127.0.0.1:1234,server,nowait

SHORT_HASH?=`git rev-parse --short HEAD`

STAGE3?=kernel
SYSTEM?=system
ARCH_X86_64_REALMODE?=realmode
STAGE3_DIR?=$(STAGE3)
SYSTEM_DIR?=$(SYSTEM)
SYSTEM_BIN=$(SYSTEM).bin
ARCH_X86_64_REALMODE_BIN=$(ARCH_X86_64_REALMODE).bin

STAGE3_INC=-I$(STAGE3)/include -I$(STAGE3)/arch/$(ARCH)/include

STAGE3_ARCH_X86_64_DIR=$(STAGE3_DIR)/arch/x86_64
STAGE3_OBJS_X86_64= $(STAGE3_DIR)/platform/acpi/acpitables.o					\
					$(STAGE3_ARCH_X86_64_DIR)/entrypoints/limine_init.o			\
					$(STAGE3_ARCH_X86_64_DIR)/entrypoints/limine_entrypoint.o	\
					$(STAGE3_ARCH_X86_64_DIR)/platform/init.o					\
					$(STAGE3_ARCH_X86_64_DIR)/entrypoints/common.o				\
					$(STAGE3_ARCH_X86_64_DIR)/machine.o							\
					$(STAGE3_ARCH_X86_64_DIR)/pic.o								\
					$(STAGE3_ARCH_X86_64_DIR)/interrupts.o						\
					$(STAGE3_ARCH_X86_64_DIR)/isr_dispatch.o					\
					$(STAGE3_ARCH_X86_64_DIR)/init_interrupts.o					\
					$(STAGE3_ARCH_X86_64_DIR)/pagefault.o						\
					$(STAGE3_ARCH_X86_64_DIR)/init_pagetables.o					\
					$(STAGE3_ARCH_X86_64_DIR)/vmm/vmmapper.o					\
					$(STAGE3_ARCH_X86_64_DIR)/gdt.o								\
					$(STAGE3_ARCH_X86_64_DIR)/general_protection_fault.o		\
					$(STAGE3_ARCH_X86_64_DIR)/double_fault.o					\
					$(STAGE3_ARCH_X86_64_DIR)/kdrivers/cpu.o					\
					$(STAGE3_ARCH_X86_64_DIR)/kdrivers/local_apic.o				\
					$(STAGE3_ARCH_X86_64_DIR)/kdrivers/hpet.o					\
					$(STAGE3_ARCH_X86_64_DIR)/init_syscalls.o					\
					$(STAGE3_ARCH_X86_64_DIR)/task_switch.o						\
					$(STAGE3_ARCH_X86_64_DIR)/task_user_entrypoint.o			\
					$(STAGE3_ARCH_X86_64_DIR)/cpuid.o							\
					$(STAGE3_ARCH_X86_64_DIR)/smp/startup.o						\
					$(STAGE3_ARCH_X86_64_DIR)/task_kernel_entrypoint.o			\
					$(STAGE3_ARCH_X86_64_DIR)/kdrivers/serial.o					\
					$(STAGE3_ARCH_X86_64_DIR)/std_routines.o					\
					$(STAGE3_ARCH_X86_64_DIR)/structs/list.o					\
					$(STAGE3_ARCH_X86_64_DIR)/spinlock.o						\
					$(STAGE3_ARCH_X86_64_DIR)/smp/ipwi_dispatcher.o				\
					$(STAGE3_ARCH_X86_64_DIR)/smp/ipwi.o						\
					$(STAGE3_ARCH_X86_64_DIR)/debugmadt.o						\
					$(STAGE3_ARCH_X86_64_DIR)/capabilities/cookies.o			\
					$(STAGE3_ARCH_X86_64_DIR)/vmm/vmmapper_init.o				\
					$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE)_linkable.o

STAGE3_ARCH_RISCV64_DIR=$(STAGE3_DIR)/arch/riscv64
STAGE3_OBJS_RISCV64=$(STAGE3_ARCH_RISCV64_DIR)/entrypoints/limine_init.o		\
					$(STAGE3_ARCH_RISCV64_DIR)/entrypoints/limine_entrypoint.o	\
					$(STAGE3_ARCH_RISCV64_DIR)/platform/init.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/sbi.o							\
					$(STAGE3_ARCH_RISCV64_DIR)/machine.o						\
					$(STAGE3_ARCH_RISCV64_DIR)/init_interrupts.o				\
					$(STAGE3_ARCH_RISCV64_DIR)/isr_dispatch.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/pagefault.o						\
					$(STAGE3_ARCH_RISCV64_DIR)/std_routines.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/vmm/vmmapper.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/structs/list.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/capabilities/cookies.o			\
					$(STAGE3_ARCH_RISCV64_DIR)/vmm/vmmapper_init.o				\
					$(STAGE3_ARCH_RISCV64_DIR)/smp/ipwi.o						\
					$(STAGE3_ARCH_RISCV64_DIR)/task_switch.o					\
					$(STAGE3_ARCH_RISCV64_DIR)/task_user_entrypoint.o			\
					$(STAGE3_ARCH_RISCV64_DIR)/task_kernel_entrypoint.o			\
					$(STAGE3_ARCH_RISCV64_DIR)/spinlock.o						\
					$(STAGE3_ARCH_RISCV64_DIR)/kdrivers/sbi.o

ifeq ($(ARCH),x86_64)
STAGE3_ARCH_OBJS=$(STAGE3_OBJS_X86_64)
else
ifeq ($(ARCH),riscv64)
STAGE3_ARCH_OBJS=$(STAGE3_OBJS_RISCV64)
else
$(error Arch $(ARCH) is not supported)
endif
endif

ifeq ($(ARCH),x86_64)
STAGE3_OBJS=$(STAGE3_DIR)/entrypoint.o											\
			$(STAGE3_DIR)/gdebugterm.o											\
			$(STAGE3_DIR)/banner.o												\
			$(STAGE3_DIR)/debugmemmap.o											\
			$(STAGE3_DIR)/kprintf.o												\
			$(STAGE3_DIR)/isr_handlers.o										\
			$(STAGE3_DIR)/pmm/pagealloc.o										\
			$(STAGE3_DIR)/fba/alloc.o											\
			$(STAGE3_DIR)/slab/alloc.o											\
			$(STAGE3_DIR)/timer_isr.o											\
			$(STAGE3_DIR)/pagefault.o											\
			$(STAGE3_DIR)/kdrivers/drivers.o									\
			$(STAGE3_DIR)/syscalls.o											\
			$(STAGE3_DIR)/task.o												\
			$(STAGE3_DIR)/sched/prr.o											\
			$(STAGE3_DIR)/structs/pq.o											\
			$(STAGE3_DIR)/sleep.o												\
			$(STAGE3_DIR)/sleep_queue.o											\
			$(STAGE3_DIR)/panic.o												\
			$(STAGE3_DIR)/system.o												\
			$(STAGE3_DIR)/sched/idle.o											\
			$(STAGE3_DIR)/sched/lock.o											\
			$(STAGE3_DIR)/structs/ref_count_map.o								\
			$(STAGE3_DIR)/process/process.o										\
			$(STAGE3_DIR)/smp/state.o											\
			$(STAGE3_DIR)/structs/hash.o										\
			$(STAGE3_DIR)/ipc/channel.o											\
			$(STAGE3_DIR)/ipc/named.o											\
			$(STAGE3_DIR)/process/memory.o										\
			$(STAGE3_DIR)/managed_resources/resources.o							\
			$(STAGE3_DIR)/capabilities/map.o									\
			$(STAGE3_DIR)/capabilities/capabilities.o							\
			$(STAGE3_DIR)/structs/region_tree.o									\
			$(STAGE3_DIR)/structs/shift_array.o									\
			$(STAGE3_DIR)/smp/ipwi.o											\
			$(STAGE3_DIR)/vmm/vmm_shootdown.o									\
			$(STAGE3_DIR)/process/address_space.o								\
			$(STAGE3_ARCH_OBJS)													\
			$(SYSTEM)_linkable.o
else
ifeq ($(ARCH),riscv64)
STAGE3_OBJS=$(STAGE3_DIR)/entrypoint.o											\
			$(STAGE3_DIR)/kprintf.o												\
			$(STAGE3_DIR)/debugmemmap.o											\
			$(STAGE3_DIR)/pmm/pagealloc.o										\
			$(STAGE3_DIR)/panic.o												\
			$(STAGE3_DIR)/gdebugterm.o											\
            $(STAGE3_DIR)/banner.o												\
			$(STAGE3_DIR)/fba/alloc.o											\
			$(STAGE3_DIR)/slab/alloc.o											\
			$(STAGE3_DIR)/pagefault.o											\
			$(STAGE3_DIR)/timer_isr.o											\
			$(STAGE3_DIR)/ipc/channel.o											\
			$(STAGE3_DIR)/ipc/named.o											\
			$(STAGE3_DIR)/structs/hash.o										\
			$(STAGE3_DIR)/syscalls.o											\
			$(STAGE3_DIR)/task.o												\
			$(STAGE3_DIR)/sched/prr.o											\
			$(STAGE3_DIR)/sched/idle.o											\
			$(STAGE3_DIR)/sched/lock.o											\
			$(STAGE3_DIR)/structs/pq.o											\
			$(STAGE3_DIR)/capabilities/map.o									\
			$(STAGE3_DIR)/capabilities/capabilities.o							\
			$(STAGE3_DIR)/structs/ref_count_map.o								\
			$(STAGE3_DIR)/sleep.o												\
			$(STAGE3_DIR)/sleep_queue.o											\
			$(STAGE3_DIR)/process/process.o										\
			$(STAGE3_DIR)/process/memory.o										\
			$(STAGE3_DIR)/system.o												\
			$(STAGE3_DIR)/smp/state.o											\
			$(STAGE3_DIR)/smp/ipwi.o											\
			$(STAGE3_DIR)/structs/shift_array.o									\
			$(STAGE3_DIR)/structs/region_tree.o									\
			$(STAGE3_DIR)/managed_resources/resources.o							\
			$(STAGE3_DIR)/process/address_space.o								\
            $(STAGE3_ARCH_OBJS)													\
        	$(SYSTEM)_linkable.o
endif
endif

ARCH_X86_64_REALMODE_OBJS=$(STAGE3_DIR)/arch/x86_64/smp/ap_trampoline.o

CLEAN_ARTIFACTS=$(STAGE3_DIR)/*.dis $(STAGE3_DIR)/*.elf $(STAGE3_DIR)/*.o 		\
	       		$(STAGE3_DIR)/pmm/*.o $(STAGE3_DIR)/vmm/*.o				 		\
				$(STAGE3_DIR)/kdrivers/*.o										\
				$(STAGE3_DIR)/fba/*.o $(STAGE3_DIR)/slab/*.o					\
				$(STAGE3_DIR)/structs/*.o $(STAGE3_DIR)/sched/*.o				\
				$(STAGE3_DIR)/smp/*.o $(STAGE3_DIR)/process/*.o					\
				$(STAGE3_DIR)/ipc/*.o											\
				$(STAGE3_DIR)/managed_resources/*.o								\
				$(STAGE3_DIR)/capabilities/*.o									\
				$(STAGE3_DIR)/platform/acpi/*.o									\
				$(SYSTEM)_linkable.o											\
		   		$(FLOPPY_IMG)													\
				$(UEFI_IMG)														\
	       		$(STAGE3_ARCH_X86_64_DIR)/*.dis $(STAGE3_ARCH_X86_64_DIR)/*.elf	\
				$(STAGE3_ARCH_X86_64_DIR)/*.o $(STAGE3_ARCH_X86_64_DIR)/pmm/*.o	\
				$(STAGE3_ARCH_X86_64_DIR)/vmm/*.o								\
				$(STAGE3_ARCH_X86_64_DIR)/kdrivers/*.o							\
				$(STAGE3_ARCH_X86_64_DIR)/structs/*.o							\
				$(STAGE3_ARCH_X86_64_DIR)/sched/*.o								\
				$(STAGE3_ARCH_X86_64_DIR)/smp/*.o								\
				$(STAGE3_ARCH_X86_64_DIR)/process/*.o							\
				$(STAGE3_ARCH_X86_64_DIR)/entrypoints/*.o						\
				$(STAGE3_ARCH_X86_64_DIR)/structs/*.o							\
				$(STAGE3_ARCH_X86_64_DIR)/capabilities/*.o						\
				$(STAGE3_ARCH_X86_64_DIR)/platform/*.o							\
				$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).bin			\
				$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE)_linkable.o	\
	       		$(STAGE3_ARCH_RISCV64_DIR)/*.dis 								\
				$(STAGE3_ARCH_RISCV64_DIR)/*.elf								\
				$(STAGE3_ARCH_RISCV64_DIR)/vmm/*.o								\
				$(STAGE3_ARCH_RISCV64_DIR)/entrypoints/*.o						\
				$(STAGE3_ARCH_RISCV64_DIR)/structs/*.o							\
				$(STAGE3_ARCH_RISCV64_DIR)/smp/*.o								\
				$(STAGE3_ARCH_RISCV64_DIR)/capabilities/*.o						\
				$(STAGE3_ARCH_RISCV64_DIR)/platform/*.o							\
				$(STAGE3_ARCH_RISCV64_DIR)/process/*.o							\
				$(STAGE3_ARCH_RISCV64_DIR)/kdrivers/*.o							\
				$(STAGE3_ARCH_RISCV64_DIR)/*.o

ifeq ($(CONSERVATIVE_BUILD),true)
CDEFS+=-DCONSERVATIVE_BUILD

ifeq ($(CONSERVATIVE_UBSAN),true)
CFLAGS+=-fsanitize=undefined
STAGE3_OBJS+=$(STAGE3_DIR)/ubsan.o
endif

ifeq ($(CONSERVATIVE_PANICKY),true)
CDEFS+=-DCONSERVATIVE_PANICKY
endif

endif

.PHONY: all clean test coverage												\
	qemu-uefi debug-qemu-uefi-start debug-qemu-uefi-start-terminal 			\
	debug-qemu-uefi

all: test build

clean:
	rm -rf $(CLEAN_ARTIFACTS)
	$(MAKE) -C system clean
	$(MAKE) -C servers clean
	$(MAKE) -C tools clean

include kernel/tests/include.mk
include system/tests/include.mk
include servers/pcidrv/tests/include.mk

test: test-kernel test-system test-pcidrv
coverage: coverage-kernel coverage-system

ifeq ($(ARCH),x86_64)
%.o: %.asm
	$(ASM) 																		\
	-DVERSTR=$(SHORT_HASH) 														\
	$(STAGE3_INC)																\
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
	$(XCC) -DVERSTR=$(SHORT_HASH) $(CDEFS) $(STAGE3_INC) $(CFLAGS) -c -o $@ $<

############## System  ##############
$(SYSTEM_DIR)/$(SYSTEM_BIN): $(SYSTEM_DIR)/Makefile
	$(MAKE) -C $(SYSTEM_DIR)

ifeq ($(ARCH),x86_64)
$(SYSTEM)_linkable.o: $(SYSTEM_DIR)/$(SYSTEM_BIN)
	$(XOBJCOPY) -I binary --rename-section .data=.$(SYSTEM)_bin -O elf64-x86-64 --binary-architecture i386:x86-64 $< $@
else
ifeq ($(ARCH),riscv64)
$(SYSTEM)_linkable.o: $(SYSTEM_DIR)/$(SYSTEM_BIN)
	$(XOBJCOPY) -I binary --rename-section .data=.$(SYSTEM)_bin -O elf64-littleriscv --binary-architecture riscv:rv64 $< $@
else
$(error Need an architecture-specific setup for objcopy:system_linkable in system/Makefile)
endif
endif

ifeq ($(ARCH),x86_64)
############# Real mode #############
$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).elf: $(ARCH_X86_64_REALMODE_OBJS)
	$(XLD) -T $(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).ld -o $@ $^
	chmod a-x $@

$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).dis: $(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).elf
	$(XOBJDUMP) -D -S -Maddr64,data64 $< > $@

$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE_BIN): $(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).elf $(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE).dis
	$(XOBJCOPY) --strip-debug -O binary $< $@
	chmod a-x $@

$(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE)_linkable.o: $(STAGE3_ARCH_X86_64_DIR)/$(ARCH_X86_64_REALMODE_BIN)
	$(XOBJCOPY) -I binary --rename-section .data=.$(ARCH_X86_64_REALMODE)_bin -O elf64-x86-64 --binary-architecture i386:x86-64 $< $@
endif

############## Stage 3 ##############
$(STAGE3_DIR)/$(STAGE3).elf: $(STAGE3_OBJS)
	$(XLD) -T $(STAGE3_DIR)/arch/$(ARCH)/$(STAGE3).ld -o $@ $^
	chmod a-x $@

$(STAGE3_DIR)/$(STAGE3).dis: $(STAGE3_DIR)/$(STAGE3).elf
	$(XOBJDUMP) -D -S -Maddr64,data64 $< > $@


# ############ UEFI Image ############
UEFI_IMG=anos_uefi.img
UEFI_BOOT_WALLPAPER?=uefi/anoschip2-glass.jpg
UEFI_CONF?=uefi/$(ARCH)/limine.conf

ifeq ($(ARCH),x86_64)
UEFI_APPLICATION=uefi/$(ARCH)/limine/BOOTX64.EFI
else
ifeq ($(ARCH),riscv64)
UEFI_APPLICATION=uefi/$(ARCH)/limine/BOOTRISCV64.EFI
endif
endif

$(UEFI_IMG): $(STAGE3_DIR)/$(STAGE3).elf $(UEFI_CONF) $(UEFI_BOOT_WALLPAPER) $(UEFI_APPLICATION)
	dd of=$@ if=/dev/zero bs=4M count=1
	mformat -T 8192 -v ANOSDISK002 -i $@ ::
	mmd -i $@ EFI
	mmd -i $@ EFI/BOOT
	mcopy -i $@ $(UEFI_CONF) ::/EFI/BOOT/limine.conf
	mcopy -i $@ $(UEFI_BOOT_WALLPAPER) ::/EFI/BOOT/anos.jpg
	mcopy -i $@ $(UEFI_APPLICATION) ::/EFI/BOOT
	mcopy -i $@ $(STAGE3_DIR)/$(STAGE3).elf ::

ifeq ($(ARCH),riscv64)
uefi/riscv64/edk2/RISCV_VIRT_VARS.fd: 
	cp $@.template $@

qemu-uefi: $(UEFI_IMG) | uefi/riscv64/edk2/RISCV_VIRT_VARS.fd
else
qemu-uefi: $(UEFI_IMG)
endif
	$(QEMU) $(QEMU_BASEOPTS) $(QEMU_UEFI_OPTS)

debug-qemu-uefi-start: $(UEFI_IMG)
	$(QEMU) $(QEMU_BASEOPTS) $(QEMU_UEFI_OPTS) $(QEMU_DEBUG_OPTS)

debug-qemu-uefi-start-terminal: $(UEFI_IMG)
	$(QEMU) $(QEMU_BASEOPTS) $(QEMU_UEFI_OPTS) $(QEMU_DEBUG_OPTS) &

debug-qemu-uefi: debug-qemu-uefi-start-terminal
	gdb -ex 'target remote localhost:9666'

build: $(UEFI_IMG)
