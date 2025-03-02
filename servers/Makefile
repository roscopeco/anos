SERVERS_ROOT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

MKRAMFS?=mkramfs
MKRAMFS_DIR?=$(SERVERS_ROOT_DIR)/../tools/$(MKRAMFS)
MKRAMFS_BIN?=$(MKRAMFS_DIR)/$(MKRAMFS)

.PHONY: all clean 

all: test_server/test_server.bin

clean:
	$(MAKE) -C test_server clean
	$(RM) start.o
	$(RM) system.ramfs

$(MKRAMFS_BIN): $(MKRAMFS_DIR)
	$(MAKE) -C $< $(MKRAMFS)

test_server/test_server.bin: server_common.mk test_server/Makefile Makefile start.asm test_server/main.c
	$(MAKE) -C test_server test_server.bin

system_ramfs: $(MKRAMFS_BIN) test_server/test_server.bin
	$(MKRAMFS_BIN) system.ramfs test_server/test_server.bin