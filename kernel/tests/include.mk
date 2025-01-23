CLEAN_ARTIFACTS+=kernel/tests/*.o kernel/tests/pmm/*.o kernel/tests/vmm/*.o 		\
				kernel/tests/structs/*.o kernel/tests/pci/*.o kernel/tests/fba/*.o 	\
				kernel/tests/slab/*.o kernel/tests/sched/*.o kernel/tests/build		\
				kernel/tests/*.gcda kernel/tests/pmm/*.gcda kernel/tests/vmm/*.gcda	\
				kernel/tests/structs/*.gcda kernel/tests/pci/*.gcda 				\
				kernel/tests/fba/*.gcda kernel/tests/slab/*.gcda 					\
				kernel/tests/sched/*.gcda kernel/tests/kdrivers/*.gcda				\
				kernel/tests/*.gcno kernel/tests/pmm/*.gcno kernel/tests/vmm/*.gcno	\
				kernel/tests/structs/*.gcno kernel/tests/pci/*.gcno 				\
				kernel/tests/fba/*.gcno kernel/tests/slab/*.gcno 					\
				kernel/tests/sched/*.gcno kernel/tests/kdrivers/*.gcno				\
				kernel/tests/build													\
				gcov

UBSAN_CFLAGS=-fsanitize=undefined -fno-sanitize-recover=all
TEST_CFLAGS=-g -Ikernel/include -Ikernel/tests/include -O3 -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=kernel/tests/build kernel/tests/build/pmm kernel/tests/build/vmm	\
				kernel/tests/build/structs kernel/tests/build/pci 					\
				kernel/tests/build/fba kernel/tests/build/slab 						\
				kernel/tests/build/sched kernel/tests/build/kdrivers

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
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

kernel/tests/build/%.o: kernel/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(TEST_CFLAGS) -c -o $@ $<

kernel/tests/build/%.o: kernel/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

kernel/tests/%.o: kernel/tests/%.c kernel/tests/munit.h
	$(CC) -DUNIT_TESTS $(TEST_CFLAGS) -Ikernel/tests -c -o $@ $<

kernel/tests/build/interrupts: kernel/tests/munit.o kernel/tests/interrupts.o kernel/tests/build/interrupts.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/bitmap: kernel/tests/munit.o kernel/tests/structs/bitmap.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/pmm/pagealloc: kernel/tests/munit.o kernel/tests/pmm/pagealloc.o kernel/tests/build/pmm/pagealloc.o kernel/tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/vmmapper: kernel/tests/munit.o kernel/tests/vmm/vmmapper.o kernel/tests/build/vmm/vmmapper.o kernel/tests/mock_pmm_malloc.o kernel/tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/vmalloc_linkedlist: kernel/tests/munit.o kernel/tests/vmm/vmalloc_linkedlist.o kernel/tests/build/vmm/vmalloc_linkedlist.o kernel/tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/debugprint: kernel/tests/munit.o kernel/tests/debugprint.o kernel/tests/build/debugprint.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/acpitables: kernel/tests/munit.o kernel/tests/acpitables.o kernel/tests/build/acpitables.o kernel/tests/mock_vmm.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/list: kernel/tests/munit.o kernel/tests/structs/list.o kernel/tests/build/structs/list.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/structs/pq: kernel/tests/munit.o kernel/tests/structs/pq.o kernel/tests/build/structs/pq.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/gdt: kernel/tests/munit.o kernel/tests/gdt.o kernel/tests/build/gdt.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/pci/bus: kernel/tests/munit.o kernel/tests/pci/bus.o kernel/tests/build/pci/bus.o kernel/tests/mock_machine.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/fba/alloc: kernel/tests/munit.o kernel/tests/fba/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/spinlock: kernel/tests/munit.o kernel/tests/spinlock.o kernel/tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/slab/alloc: kernel/tests/munit.o kernel/tests/slab/alloc.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/spinlock.o kernel/tests/build/structs/list.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/vmm/recursive: kernel/tests/munit.o kernel/tests/vmm/recursive.o $(TEST_BUILD_DIRS)
	$(CC) $(TEST_CFLAGS) -o $@ kernel/tests/munit.o kernel/tests/vmm/recursive.o

kernel/tests/build/sched/rr: kernel/tests/munit.o kernel/tests/sched/rr.o kernel/tests/build/sched/rr.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/spinlock.o kernel/tests/build/structs/list.o kernel/tests/mock_task.o kernel/tests/mock_user_entrypoint.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_task.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/sched/prr: kernel/tests/munit.o kernel/tests/sched/prr.o kernel/tests/build/sched/prr.o kernel/tests/build/slab/alloc.o kernel/tests/build/fba/alloc.o kernel/tests/build/spinlock.o kernel/tests/build/structs/list.o kernel/tests/build/structs/pq.o kernel/tests/mock_task.o kernel/tests/mock_user_entrypoint.o kernel/tests/mock_pmm_noalloc.o kernel/tests/mock_vmm.o kernel/tests/mock_task.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/sched/lock: kernel/tests/munit.o kernel/tests/sched/lock.o kernel/tests/build/sched/lock.o kernel/tests/mock_spinlock.o kernel/tests/mock_machine.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/printdec: kernel/tests/munit.o kernel/tests/printdec.o kernel/tests/build/printdec.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/kdrivers/drivers: kernel/tests/munit.o kernel/tests/kdrivers/drivers.o kernel/tests/build/kdrivers/drivers.o kernel/tests/mock_kernel_drivers.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

kernel/tests/build/kdrivers/hpet: kernel/tests/munit.o kernel/tests/kdrivers/hpet.o kernel/tests/build/kdrivers/hpet.o kernel/tests/build/kdrivers/drivers.o kernel/tests/mock_acpitables.o kernel/tests/mock_vmm.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

ALL_TESTS=kernel/tests/build/interrupts 										\
			kernel/tests/build/structs/bitmap									\
			kernel/tests/build/pmm/pagealloc									\
			kernel/tests/build/vmm/vmmapper										\
			kernel/tests/build/vmm/vmalloc_linkedlist							\
			kernel/tests/build/debugprint										\
			kernel/tests/build/acpitables										\
			kernel/tests/build/structs/list										\
			kernel/tests/build/structs/pq										\
			kernel/tests/build/gdt												\
			kernel/tests/build/pci/bus											\
			kernel/tests/build/fba/alloc										\
			kernel/tests/build/spinlock											\
			kernel/tests/build/slab/alloc										\
			kernel/tests/build/vmm/recursive									\
			kernel/tests/build/sched/lock										\
			kernel/tests/build/sched/rr											\
			kernel/tests/build/sched/prr										\
			kernel/tests/build/printdec											\
			kernel/tests/build/kdrivers/drivers									\
			kernel/tests/build/kdrivers/hpet

PHONY: test
test: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage:
	@echo "LCOV not installed, coverage cannot be generated"
else
PHONY: gcov

gcov/kernel:
	mkdir -p $@

coverage: test gcov/kernel
	lcov --capture --directory kernel/tests --output-file gcov/kernel/coverage.info
	genhtml gcov/kernel/coverage.info --output-directory gcov/kernel
endif

