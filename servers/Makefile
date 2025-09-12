SERVERS_ROOT_DIR=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

MKRAMFS?=mkramfs
MKRAMFS_DIR?=$(SERVERS_ROOT_DIR)/../tools/$(MKRAMFS)
MKRAMFS_BIN?=$(MKRAMFS_DIR)/$(MKRAMFS)

.PHONY: all clean 

all: test_server/test_server.elf devman/devman.elf pcidrv/pcidrv.elf ahcidrv/ahcidrv.elf fat32drv/fat32drv.elf

clean:
	$(MAKE) -C test_server clean
	$(MAKE) -C devman clean
	$(MAKE) -C pcidrv clean
	$(MAKE) -C ahcidrv clean
	$(MAKE) -C fat32drv clean
	$(RM) system.ramfs

$(MKRAMFS_BIN): $(MKRAMFS_DIR)
	$(MAKE) -C $< $(MKRAMFS)

test_server/test_server.elf: server_common.mk test_server/Makefile Makefile test_server/main.c
	$(MAKE) -C test_server test_server.elf

devman/devman.elf: server_common.mk devman/Makefile Makefile devman/main.c
	$(MAKE) -C devman devman.elf

pcidrv/pcidrv.elf: server_common.mk pcidrv/Makefile Makefile pcidrv/main.c
	$(MAKE) -C pcidrv pcidrv.elf

ahcidrv/ahcidrv.elf: server_common.mk ahcidrv/Makefile Makefile ahcidrv/main.c
	$(MAKE) -C ahcidrv ahcidrv.elf

fat32drv/fat32drv.elf: server_common.mk fat32drv/Makefile Makefile fat32drv/main.c
	$(MAKE) -C fat32drv fat32drv.elf

ifeq ($(ARCH),x86_64)
system_ramfs: $(MKRAMFS_BIN) test_server/test_server.elf devman/devman.elf pcidrv/pcidrv.elf ahcidrv/ahcidrv.elf fat32drv/fat32drv.elf
	$(MKRAMFS_BIN) system.ramfs test_server/test_server.elf devman/devman.elf pcidrv/pcidrv.elf ahcidrv/ahcidrv.elf fat32drv/fat32drv.elf
else
system_ramfs: $(MKRAMFS_BIN) test_server/test_server.elf
	$(MKRAMFS_BIN) system.ramfs test_server/test_server.elf
endif