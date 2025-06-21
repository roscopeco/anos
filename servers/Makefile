SERVERS_ROOT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

MKRAMFS?=mkramfs
MKRAMFS_DIR?=$(SERVERS_ROOT_DIR)/../tools/$(MKRAMFS)
MKRAMFS_BIN?=$(MKRAMFS_DIR)/$(MKRAMFS)

.PHONY: all clean 

all: test_server/test_server.elf devman/devman.elf

clean:
	$(MAKE) -C test_server clean
	$(MAKE) -C devman clean
	$(RM) system.ramfs

$(MKRAMFS_BIN): $(MKRAMFS_DIR)
	$(MAKE) -C $< $(MKRAMFS)

test_server/test_server.elf: server_common.mk test_server/Makefile Makefile test_server/main.c
	$(MAKE) -C test_server test_server.elf

devman/devman.elf: server_common.mk devman/Makefile Makefile devman/main.c
	$(MAKE) -C devman devman.elf

system_ramfs: $(MKRAMFS_BIN) test_server/test_server.elf devman/devman.elf
	$(MKRAMFS_BIN) system.ramfs test_server/test_server.elf devman/devman.elf