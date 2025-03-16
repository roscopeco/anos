CLEAN_ARTIFACTS+=kernel/tests/*.o kernel/tests/pmm/*.o kernel/tests/vmm/*.o 		\
				kernel/tests/structs/*.o kernel/tests/pci/*.o kernel/tests/fba/*.o 	\
				kernel/tests/slab/*.o kernel/tests/sched/*.o kernel/tests/smp/*.o	\
				kernel/tests/kdrivers/*.o											\
				kernel/tests/ipc/*.o												\
				kernel/tests/arch/x86_64/*.o										\
				kernel/tests/arch/x86_64/sched/*.o									\
				kernel/tests/arch/x86_64/kdrivers/*.o								\
				kernel/tests/arch/x86_64/process/*.o								\
				kernel/tests/arch/x86_64/structs/*.o								\
				kernel/tests/*.gcda kernel/tests/pmm/*.gcda kernel/tests/vmm/*.gcda	\
				kernel/tests/structs/*.gcda kernel/tests/pci/*.gcda 				\
				kernel/tests/fba/*.gcda kernel/tests/slab/*.gcda 					\
				kernel/tests/sched/*.gcda kernel/tests/smp/*.gcda					\
				kernel/tests/kdrivers/*.gcda										\
				kernel/tests/ipc/*.gcda												\
				kernel/tests/arch/x86_64/*.gcda										\
				kernel/tests/arch/x86_64/sched/*.gcda								\
				kernel/tests/arch/x86_64/kdrivers/*.gcda							\
				kernel/tests/arch/x86_64/process/*.gcda								\
				kernel/tests/arch/x86_64/structs/*.gcda								\
				kernel/tests/*.gcno kernel/tests/pmm/*.gcno kernel/tests/vmm/*.gcno	\
				kernel/tests/structs/*.gcno kernel/tests/pci/*.gcno 				\
				kernel/tests/fba/*.gcno kernel/tests/slab/*.gcno 					\
				kernel/tests/sched/*.gcno kernel/tests/smp/*.gcno					\
				kernel/tests/kdrivers/*.gcno										\
				kernel/tests/ipc/*.gcno												\
				kernel/tests/arch/x86_64/*.gcno										\
				kernel/tests/arch/x86_64/sched/*.gcno								\
				kernel/tests/arch/x86_64/kdrivers/*.gcno							\
				kernel/tests/arch/x86_64/process/*.gcno								\
				kernel/tests/arch/x86_64/structs/*.gcno								\
				kernel/tests/build													\
				gcov/kernel

UBSAN_CFLAGS=-fsanitize=undefined,address -fno-sanitize-recover=all
KERNEL_TEST_CFLAGS=-g 																\
	-Ikernel/include 																\
	-Ikernel/arch/$(ARCH)/include 													\
	-Ikernel/tests/include 															\
	-Ikernel/tests/arch/$(ARCH)/include 											\
	-O3 -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
KERNEL_TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=kernel/tests/build kernel/tests/build/pmm kernel/tests/build/vmm	\
				kernel/tests/build/structs kernel/tests/build/pci 					\
				kernel/tests/build/fba kernel/tests/build/slab 						\
				kernel/tests/build/sched kernel/tests/build/kdrivers				\
				kernel/tests/build/smp												\
				kernel/tests/build/arch/x86_64										\
				kernel/tests/build/arch/x86_64/sched								\
				kernel/tests/build/process											\
				kernel/tests/build/arch/x86_64/process								\
				kernel/tests/build/arch/x86_64/structs								\
				kernel/tests/build/arch/x86_64/kdrivers								\
				kernel/tests/build/ipc

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
KERNEL_TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
endif


kernel/tests/build:
	mkdir -p kernel/tests/build

kernel/tests/build/pmm:
	mkdir -p kernel/tests/build/pmm

kernel/tests/build/vmm:
	mkdir -p kernel/tests/build/vmm

kernel/tests/build/structs:
	mkdir -p kernel/tests/build/structs

kernel/tests/build/pci:
	mkdir -p kernel/tests/build/pci

kernel/tests/build/fba:
	mkdir -p kernel/tests/build/fba

kernel/tests/build/slab:
	mkdir -p kernel/tests/build/slab

kernel/tests/build/sched:
	mkdir -p kernel/tests/build/sched

kernel/tests/build/kdrivers:
	mkdir -p kernel/tests/build/kdrivers

kernel/tests/build/smp:
	mkdir -p kernel/tests/build/smp

kernel/tests/build/ipc:
	mkdir -p kernel/tests/build/ipc

kernel/tests/build/arch/x86_64:
	mkdir -p kernel/tests/build/arch/x86_64

kernel/tests/build/arch/x86_64/sched:
	mkdir -p kernel/tests/build/arch/x86_64/sched

kernel/tests/build/arch/x86_64/kdrivers:
	mkdir -p kernel/tests/build/arch/x86_64/kdrivers

kernel/tests/build/arch/x86_64/process:
	mkdir -p kernel/tests/build/arch/x86_64/process

kernel/tests/build/arch/x86_64/structs:
	mkdir -p kernel/tests/build/arch/x86_64/structs

kernel/tests/build/process:
	mkdir -p kernel/tests/build/process

kernel/tests/build/%.o: kernel/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(KERNEL_TEST_CFLAGS) -c -o $@ $<

kernel/tests/build/%.o: kernel/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

kernel/tests/%.o: kernel/tests/%.c kernel/tests/munit.h
	$(CC) -DUNIT_TESTS $(KERNEL_TEST_CFLAGS) -Ikernel/tests -c -o $@ $<

kernel/tests/build/interrupts: kernel/tests/munit.o kernel/tests/arch/x86_64/interrupts.o kernel/tests/build/arch/x86_64/interrupts.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/bitmap: kernel/tests/munit.o kernel/tests/structs/bitmap.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/pmm/pagealloc: kernel/tests/munit.o kernel/tests/pmm/pagealloc.o kernel/tests/build/pmm/pagealloc.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/pmm/pagealloc_limine: kernel/tests/munit.o kernel/tests/pmm/pagealloc_limine.o kernel/tests/build/pmm/pagealloc.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/vmmapper: kernel/tests/munit.o kernel/tests/vmm/vmmapper.o kernel/tests/build/vmm/vmmapper.o kernel/tests/mock_pmm_malloc.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/vmalloc_linkedlist: kernel/tests/munit.o kernel/tests/vmm/vmalloc_linkedlist.o kernel/tests/build/vmm/vmalloc_linkedlist.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/debugprint: kernel/tests/munit.o kernel/tests/debugprint.o kernel/tests/build/debugprint.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/acpitables: kernel/tests/munit.o kernel/tests/arch/x86_64/acpitables.o kernel/tests/build/arch/x86_64/acpitables.o kernel/tests/mock_vmm.o kernel/tests/arch/x86_64/mock_machine.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/pq: kernel/tests/munit.o kernel/tests/structs/pq.o kernel/tests/build/structs/pq.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/gdt: kernel/tests/munit.o kernel/tests/arch/x86_64/gdt.o kernel/tests/build/arch/x86_64/gdt.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/pci/bus: kernel/tests/munit.o kernel/tests/pci/bus.o kernel/tests/build/pci/bus.o kernel/tests/arch/x86_64/mock_machine.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/fba/alloc: kernel/tests/munit.o kernel/tests/fba/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/slab/alloc: kernel/tests/munit.o kernel/tests/slab/alloc.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/arch/x86_64/structs/list.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/recursive: kernel/tests/munit.o kernel/tests/vmm/recursive.o $(TEST_BUILD_DIRS)
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ kernel/tests/munit.o kernel/tests/vmm/recursive.o

kernel/tests/build/task: kernel/tests/munit.o kernel/tests/task.o kernel/tests/build/task.o kernel/tests/build/arch/x86_64/structs/list.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_user_entrypoint.o kernel/tests/mock_kernel_entrypoint.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/sched/prr: kernel/tests/munit.o kernel/tests/sched/prr.o kernel/tests/build/sched/prr.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/arch/x86_64/structs/list.o kernel/tests/build/structs/pq.o kernel/tests/build/sched/idle.o kernel/tests/build/process/process.o kernel/tests/mock_user_entrypoint.o kernel/tests/mock_kernel_entrypoint.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_task.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/sched/lock: kernel/tests/munit.o kernel/tests/arch/x86_64/sched/lock.o kernel/tests/build/arch/x86_64/sched/lock.o kernel/tests/mock_spinlock.o kernel/tests/arch/x86_64/mock_machine.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/printdec: kernel/tests/munit.o kernel/tests/printdec.o kernel/tests/build/printdec.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/kdrivers/drivers: kernel/tests/munit.o kernel/tests/kdrivers/drivers.o kernel/tests/build/kdrivers/drivers.o kernel/tests/mock_kernel_drivers.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/sleep_queue: kernel/tests/munit.o kernel/tests/sleep_queue.o kernel/tests/build/sleep_queue.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/arch/x86_64/structs/list.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/ref_count_map: kernel/tests/munit.o kernel/tests/structs/ref_count_map.o kernel/tests/build/structs/ref_count_map.o kernel/tests/mock_fba_malloc.o kernel/tests/mock_slab_malloc.o kernel/tests/mock_spinlock.o kernel/tests/arch/x86_64/mock_machine.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/hash: kernel/tests/munit.o kernel/tests/structs/hash.o kernel/tests/build/structs/hash.o kernel/tests/build/arch/x86_64/spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/ipc/channel: kernel/tests/munit.o kernel/tests/ipc/channel.o kernel/tests/build/ipc/channel.o kernel/tests/build/structs/hash.o kernel/tests/mock_fba_malloc.o kernel/tests/mock_vmm.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/strhash: kernel/tests/munit.o kernel/tests/structs/strhash.o $(TEST_BUILD_DIRS)
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ kernel/tests/munit.o kernel/tests/structs/strhash.o

kernel/tests/build/ipc/named: kernel/tests/munit.o kernel/tests/ipc/named.o kernel/tests/build/ipc/named.o kernel/tests/build/structs/hash.o kernel/tests/mock_fba_malloc.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/shift_array: kernel/tests/munit.o kernel/tests/structs/shift_array.o kernel/tests/build/structs/shift_array.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/spinlock: kernel/tests/munit.o kernel/tests/arch/x86_64/spinlock.o kernel/tests/build/arch/x86_64/spinlock.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/structs/list: kernel/tests/munit.o kernel/tests/arch/x86_64/structs/list.o kernel/tests/build/arch/x86_64/structs/list.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/kdrivers/hpet: kernel/tests/munit.o kernel/tests/arch/x86_64/kdrivers/hpet.o kernel/tests/build/arch/x86_64/kdrivers/hpet.o kernel/tests/build/kdrivers/drivers.o kernel/tests/arch/x86_64/mock_acpitables.o kernel/tests/mock_vmm.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/process/address_space_init: kernel/tests/munit.o kernel/tests/arch/x86_64/process/address_space_init.o kernel/tests/build/arch/x86_64/process/address_space.o kernel/tests/mock_pmm_malloc.o kernel/tests/mock_vmm.o kernel/tests/mock_spinlock.o kernel/tests/arch/x86_64/mock_machine.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/process/address_space_create: kernel/tests/munit.o kernel/tests/arch/x86_64/process/address_space_create.o kernel/tests/build/arch/x86_64/process/address_space.o kernel/tests/mock_pmm_malloc.o kernel/tests/mock_spinlock.o kernel/tests/arch/x86_64/mock_machine.o kernel/tests/mock_vmm.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

kernel/tests/build/arch/x86_64/std_routines: kernel/tests/munit.o kernel/tests/arch/x86_64/std_routines.o kernel/tests/build/arch/x86_64/std_routines.o
	$(CC) $(KERNEL_TEST_CFLAGS) -o $@ $^

ALL_TESTS=kernel/tests/build/interrupts 										\
			kernel/tests/build/structs/bitmap									\
			kernel/tests/build/pmm/pagealloc									\
			kernel/tests/build/pmm/pagealloc_limine								\
			kernel/tests/build/vmm/vmmapper										\
			kernel/tests/build/vmm/vmalloc_linkedlist							\
			kernel/tests/build/debugprint										\
			kernel/tests/build/acpitables										\
			kernel/tests/build/structs/pq										\
			kernel/tests/build/gdt												\
			kernel/tests/build/pci/bus											\
			kernel/tests/build/fba/alloc										\
			kernel/tests/build/slab/alloc										\
			kernel/tests/build/vmm/recursive									\
			kernel/tests/build/arch/x86_64/sched/lock							\
			kernel/tests/build/task												\
			kernel/tests/build/sched/prr										\
			kernel/tests/build/printdec											\
			kernel/tests/build/kdrivers/drivers									\
			kernel/tests/build/sleep_queue										\
			kernel/tests/build/structs/ref_count_map							\
			kernel/tests/build/structs/hash										\
			kernel/tests/build/ipc/channel										\
			kernel/tests/build/structs/strhash									\
			kernel/tests/build/ipc/named										\
			kernel/tests/build/structs/shift_array								\
			kernel/tests/build/arch/x86_64/spinlock								\
			kernel/tests/build/arch/x86_64/structs/list							\
			kernel/tests/build/arch/x86_64/kdrivers/hpet						\
			kernel/tests/build/arch/x86_64/process/address_space_init			\
			kernel/tests/build/arch/x86_64/process/address_space_create			\
			kernel/tests/build/arch/x86_64/std_routines

PHONY: test-kernel
test-kernel: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage-kernel:
	@echo "LCOV not installed, coverage cannot be generated"
else
PHONY: gcov

gcov/kernel:
	mkdir -p $@

coverage-kernel: test-kernel gcov/kernel
	lcov --capture --directory kernel/tests --output-file gcov/kernel/coverage.info
	genhtml gcov/kernel/coverage.info --output-directory gcov/kernel
endif

