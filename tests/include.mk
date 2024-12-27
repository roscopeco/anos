CLEAN_ARTIFACTS+=tests/*.o tests/pmm/*.o tests/vmm/*.o tests/structs/*.o tests/pci/*.o tests/fba/*.o tests/slab/*.o tests/build
UBSAN_CFLAGS=-fsanitize=undefined -fno-sanitize-recover=all
TEST_CFLAGS=-g -Ikernel/include -Itests/include -O3 $(UBSAN_CFLAGS)
HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=tests/build tests/build/pmm tests/build/vmm tests/build/structs tests/build/pci tests/build/fba tests/build/slab

tests/%.o: tests/%.c tests/munit.h
	$(CC) -DUNIT_TESTS $(TEST_CFLAGS) -Itests -c -o $@ $<

tests/build:
	mkdir -p tests/build

tests/build/pmm:
	mkdir -p tests/build/pmm

tests/build/vmm:
	mkdir -p tests/build/vmm

tests/build/structs:
	mkdir -p tests/build/structs

tests/build/pci:
	mkdir -p tests/build/pci

tests/build/fba:
	mkdir -p tests/build/fba

tests/build/slab:
	mkdir -p tests/build/slab

tests/build/%.o: kernel/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(TEST_CFLAGS) -c -o $@ $<

tests/build/%.o: kernel/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

tests/build/interrupts: tests/munit.o tests/interrupts.o tests/build/interrupts.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/structs/bitmap: tests/munit.o tests/structs/bitmap.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/pmm/pagealloc: tests/munit.o tests/pmm/pagealloc.o tests/build/pmm/pagealloc.o tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/vmm/vmmapper: tests/munit.o tests/vmm/vmmapper.o tests/build/vmm/vmmapper.o tests/test_pmm_malloc.o tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/vmm/vmalloc_linkedlist: tests/munit.o tests/vmm/vmalloc_linkedlist.o tests/build/vmm/vmalloc_linkedlist.o tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/debugprint: tests/munit.o tests/debugprint.o tests/build/debugprint.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/acpitables: tests/munit.o tests/acpitables.o tests/build/acpitables.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/structs/list: tests/munit.o tests/structs/list.o tests/build/structs/list.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/gdt: tests/munit.o tests/gdt.o tests/build/gdt.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/pci/bus: tests/munit.o tests/pci/bus.o tests/build/pci/bus.o tests/test_machine.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/fba/alloc: tests/munit.o tests/fba/alloc.o tests/build/fba/alloc.o tests/test_pmm_noalloc.o tests/test_vmm.o tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/spinlock: tests/munit.o tests/spinlock.o tests/build/spinlock.o
	$(CC) $(TEST_CFLAGS) -o $@ $^

tests/build/slab/alloc: tests/munit.o tests/slab/alloc.o tests/build/slab/alloc.o tests/test_pmm_noalloc.o tests/test_vmm.o
	$(CC) $(TEST_CFLAGS) -o $@ $^


ALL_TESTS=tests/build/interrupts 										\
			tests/build/structs/bitmap									\
			tests/build/pmm/pagealloc									\
			tests/build/vmm/vmmapper									\
			tests/build/vmm/vmalloc_linkedlist							\
			tests/build/debugprint										\
			tests/build/acpitables										\
			tests/build/structs/list									\
			tests/build/gdt												\
			tests/build/pci/bus											\
			tests/build/fba/alloc										\
			tests/build/spinlock										\
			tests/build/slab/alloc

test: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

