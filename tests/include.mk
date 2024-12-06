FLOPPY_DEPENDENCIES+=test
CLEAN_ARTIFACTS+=tests/*.o tests/pmm/*.o tests/vmm/*.o tests/structs/*.o tests/build
	
tests/%.o: tests/%.c tests/munit.h
	$(CC) -DUNIT_TESTS -g -Ikernel/include -Itests -c -o $@ $<

tests/build:
	mkdir -p tests/build

tests/build/pmm:
	mkdir -p tests/build/pmm

tests/build/vmm:
	mkdir -p tests/build/vmm

tests/build/structs:
	mkdir -p tests/build/structs

tests/build/%.o: kernel/%.c tests/build tests/build/pmm tests/build/vmm tests/build/structs
	$(CC) -DUNIT_TESTS -g -Ikernel/include -c -o $@ $<

tests/build/%.o: kernel/%.asm tests/build tests/build/pmm tests/build/vmm tests/build/structs
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

tests/build/interrupts: tests/munit.o tests/interrupts.o tests/build/interrupts.o
	$(CC) -o $@ $^

tests/build/pmm/bitmap: tests/munit.o tests/pmm/bitmap.o tests/build/pmm/bitmap.o
	$(CC) -o $@ $^

tests/build/pmm/pagealloc: tests/munit.o tests/pmm/pagealloc.o tests/build/pmm/pagealloc.o
	$(CC) -o $@ $^

tests/build/vmm/vmmapper: tests/munit.o tests/vmm/vmmapper.o tests/build/vmm/vmmapper.o
	$(CC) -o $@ $^

tests/build/debugprint: tests/munit.o tests/debugprint.o tests/build/debugprint.o
	$(CC) -o $@ $^

tests/build/acpitables: tests/munit.o tests/acpitables.o tests/build/acpitables.o
	$(CC) -o $@ $^

tests/build/structs/list: tests/munit.o tests/structs/list.o tests/build/structs/list.o
	$(CC) -o $@ $^


ALL_TESTS=tests/build/interrupts 										\
			tests/build/pmm/bitmap 										\
			tests/build/pmm/pagealloc									\
			tests/build/vmm/vmmapper									\
			tests/build/debugprint										\
			tests/build/acpitables										\
			tests/build/structs/list

test: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test; done'

