CLEAN_ARTIFACTS+=servers/pcidrv/tests/*.o													\
				servers/pcidrv/tests/*.gcda													\
				servers/pcidrv/tests/*.gcno													\
				servers/pcidrv/tests/build													\
				gcov/pcidrv

UBSAN_CFLAGS=-fsanitize=undefined,address -fno-sanitize-recover=all
PCIDRV_TEST_CFLAGS=-g 																		\
	-Iservers/pcidrv/include 																\
	-Iservers/pcidrv/tests/include 															\
	-O$(OPTIMIZE) -DCONSERVATIVE_BUILD $(UBSAN_CFLAGS)

HOST_ARCH=$(shell uname -p)

ifeq ($(HOST_ARCH),arm)
PCIDRV_TEST_CFLAGS+=-arch x86_64
endif

TEST_BUILD_DIRS=servers/pcidrv/tests/build

ifeq (, $(shell which lcov))
$(warning LCOV not installed, coverage will be skipped)
else
PCIDRV_TEST_CFLAGS+=-fprofile-arcs -ftest-coverage
endif

servers/pcidrv/tests/build:
	mkdir -p servers/pcidrv/tests/build

servers/pcidrv/tests/build/%.o: servers/pcidrv/%.c $(TEST_BUILD_DIRS)
	$(CC) -DUNIT_TESTS $(PCIDRV_TEST_CFLAGS) -Iservers/pcidrv/tests -c -o $@ $<

servers/pcidrv/tests/build/%.o: servers/pcidrv/%.asm $(TEST_BUILD_DIRS)
	$(ASM) -DUNIT_TESTS -f $(HOST_OBJFORMAT) -Dasm_$(HOST_OBJFORMAT) -F dwarf -g -o $@ $<

servers/pcidrv/tests/%.o: servers/pcidrv/tests/%.c servers/pcidrv/tests/munit.h
	$(CC) -DUNIT_TESTS $(PCIDRV_TEST_CFLAGS) -Iservers/pcidrv/tests -c -o $@ $<

servers/pcidrv/tests/build/pci: servers/pcidrv/tests/munit.o servers/pcidrv/tests/pci.o servers/pcidrv/tests/build/pci.o servers/pcidrv/tests/build/enumerate.o
	$(CC) $(PCIDRV_TEST_CFLAGS) -o $@ $^

ALL_TESTS=servers/pcidrv/tests/build/pci

.PHONY: test-pcidrv
test-pcidrv: $(ALL_TESTS)
	sh -c 'for test in $^; do $$test || exit 1; done'

ifeq (, $(shell which lcov))
coverage-pcidrv:
	@echo "LCOV not installed, coverage cannot be generated"
else
.PHONY: gcov/pcidrv

gcov/pcidrv:
	mkdir -p $@

coverage-pcidrv: test-pcidrv gcov/pcidrv
	lcov --capture --directory servers/pcidrv/tests --output-file gcov/pcidrv/coverage.info
	genhtml gcov/pcidrv/coverage.info --output-directory gcov/pcidrv
endif

