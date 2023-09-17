FLOPPY_DEPENDENCIES+=test
CLEAN_ARTIFACTS+=tests/*.o tests/interrupts
	
tests/%.o: tests/%.c tests/munit.h
	$(CC) -DUNIT_TESTS -Ikernel/include -c -o $@ $<

tests/build:
	mkdir -p tests/build

tests/build/%.o: kernel/%.c tests/build
	$(CC) -DUNIT_TESTS -Ikernel/include -c -o $@ $<

tests/interrupts: tests/munit.o tests/interrupts.o tests/build/interrupts.o
	$(CC) -o $@ $^

test: tests/interrupts
	$<

