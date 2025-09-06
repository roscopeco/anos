CLEAN_ARTIFACTS+=servers/devman/tests/*.o						\
				servers/devman/tests/*.gcda						\
				servers/devman/tests/*.gcno						\
				servers/devman/tests/build						\
				gcov/devman

UBSAN_CFLAGS=-fsanitize=undefined,address -fno-sanitize-recover=all
DEVMAN_TEST_CFLAGS=-g 									\
	-Iservers/devman/include 							\
	-Iservers/devman/tests/include 						\
	-Iservers/common									\
	-O$(OPTIMIZE) -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
DEVMAN_TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=servers/devman/tests/build

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
DEVMAN_TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
endif

servers/devman/tests/build:
	mkdir -p servers/devman/tests/build

servers/devman/tests/build/%.o: servers/devman/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(DEVMAN_TEST_CFLAGS) -Iservers/devman/tests -c -o $@ $<

servers/devman/tests/build/%.o: servers/devman/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

servers/devman/tests/%.o: servers/devman/tests/%.c servers/devman/tests/munit.h
	$(CC) -DUNIT_TESTS $(DEVMAN_TEST_CFLAGS) -Iservers/devman/tests -c -o $@ $<

servers/devman/tests/build/devman: servers/devman/tests/munit.o servers/devman/tests/devman.o servers/devman/tests/build/device_registry.o
	$(CC) $(DEVMAN_TEST_CFLAGS) -o $@ $^

ALL_TESTS=servers/devman/tests/build/devman

.PHONY: test-devman
test-devman: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage-devman:
	@echo "LCOV not installed, coverage cannot be generated"
else
.PHONY: gcov/devman

gcov/devman:
	mkdir -p $@

coverage-devman: test-devman gcov/devman
	lcov --capture --directory servers/devman/tests --output-file gcov/devman/coverage.info
	genhtml gcov/devman/coverage.info --output-directory gcov/devman
endif