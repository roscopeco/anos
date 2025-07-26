CLEAN_ARTIFACTS+=servers/ahcidrv/tests/*.o													\
				servers/ahcidrv/tests/*.gcda													\
				servers/ahcidrv/tests/*.gcno													\
				servers/ahcidrv/tests/build													\
				gcov/ahcidrv

UBSAN_CFLAGS=-fsanitize=undefined,address -fno-sanitize-recover=all
AHCIDRV_TEST_CFLAGS=-g 																		\
	-Iservers/ahcidrv/include 																\
	-Iservers/ahcidrv/tests/include 															\
	-O$(OPTIMIZE) -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

TEST_STUB_INCLUDES=-Iservers/ahcidrv/tests

ifeq ($(HOST_ARCH),arm)
AHCIDRV_TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=servers/ahcidrv/tests/build

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
AHCIDRV_TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
endif

servers/ahcidrv/tests/build:
	mkdir -p servers/ahcidrv/tests/build

servers/ahcidrv/tests/build/%.o: servers/ahcidrv/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(TEST_STUB_INCLUDES) $(AHCIDRV_TEST_CFLAGS) -c -o $@ $<

servers/ahcidrv/tests/build/%.o: servers/ahcidrv/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

servers/ahcidrv/tests/%.o: servers/ahcidrv/tests/%.c servers/ahcidrv/tests/munit.h
	$(CC) -DUNIT_TESTS $(AHCIDRV_TEST_CFLAGS) -Iservers/ahcidrv/tests -c -o $@ $<

servers/ahcidrv/tests/build/ahci_driver: servers/ahcidrv/tests/munit.o servers/ahcidrv/tests/mock_syscalls.o servers/ahcidrv/tests/ahci_driver.o servers/ahcidrv/tests/build/ahci.o
	$(CC) $(AHCIDRV_TEST_CFLAGS) -o $@ $^

ALL_TESTS=servers/ahcidrv/tests/build/ahci_driver

.PHONY: test-ahcidrv
test-ahcidrv: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage-ahcidrv:
	@echo "LCOV not installed, coverage cannot be generated"
else
.PHONY: gcov/ahcidrv

gcov/ahcidrv:
	mkdir -p $@

coverage-ahcidrv: test-ahcidrv gcov/ahcidrv
	lcov --capture --directory servers/ahcidrv/tests --output-file gcov/ahcidrv/coverage.info
	genhtml gcov/ahcidrv/coverage.info --output-directory gcov/ahcidrv
endif

