CLEAN_ARTIFACTS+=system/tests/*.o													\
				system/tests/*.gcda													\
				system/tests/*.gcno													\
				system/tests/build													\
				gcov/system

UBSAN_CFLAGS=-fsanitize=undefined,address -fno-sanitize-recover=all
SYSTEM_TEST_CFLAGS=-g 																\
	-Isystem/include 																\
	-Isystem/tests/include 															\
	-O$(OPTIMIZE) -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
SYSTEM_TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=system/tests/build

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
SYSTEM_TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
endif

system/tests/build:
	mkdir -p system/tests/build

system/tests/build/%.o: system/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(SYSTEM_TEST_CFLAGS) -c -o $@ $<

system/tests/build/%.o: system/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

system/tests/%.o: system/tests/%.c system/tests/munit.h
	$(CC) -DUNIT_TESTS $(SYSTEM_TEST_CFLAGS) -Isystem/tests -c -o $@ $<

system/tests/build/ramfs: system/tests/munit.o system/tests/ramfs.o system/tests/build/ramfs.o
	$(CC) $(SYSTEM_TEST_CFLAGS) -o $@ $^

system/tests/build/path: system/tests/munit.o system/tests/path.o system/tests/build/path.o
	$(CC) $(SYSTEM_TEST_CFLAGS) -o $@ $^

ALL_TESTS=system/tests/build/ramfs system/tests/build/path

PHONY: test-system
test-system: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage-system:
	@echo "LCOV not installed, coverage cannot be generated"
else
PHONY: gcov

gcov/system:
	mkdir -p $@

coverage-system: test-system gcov/system
	lcov --capture --directory system/tests --output-file gcov/system/coverage.info
	genhtml gcov/system/coverage.info --output-directory gcov/system
endif

