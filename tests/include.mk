FLOPPY_DEPENDENCIES+=test
CLEAN_ARTIFACTS+=tests/*.o tests/pmm/*.o tests/build
	
tests/%.o: tests/%.c tests/munit.h
	$(CC) -DUNIT_TESTS -g -Ikernel/include -Itests -c -o $@ $<

tests/build:
	mkdir -p tests/build

tests/build/pmm:
	mkdir -p tests/build/pmm	

tests/build/%.o: kernel/%.c tests/build tests/build/pmm
	$(CC) -DUNIT_TESTS -g -Ikernel/include -c -o $@ $<

tests/build/interrupts: tests/munit.o tests/interrupts.o tests/build/interrupts.o
	$(CC) -o $@ $^

tests/build/pmm/bitmap: tests/munit.o tests/pmm/bitmap.o tests/build/pmm/bitmap.o
	$(CC) -o $@ $^

tests/build/pmm/pagealloc: tests/munit.o tests/pmm/pagealloc.o tests/build/pmm/pagealloc.o
	$(CC) -o $@ $^

test: tests/build/interrupts tests/build/pmm/bitmap tests/build/pmm/pagealloc
	sh -c 'for test in $^; do $$test; done'

