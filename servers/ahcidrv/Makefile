SERVER_NAME=ahcidrv
BINARY=ahcidrv
BINARY_OBJS=main.o ahci.o

# The following C defines are recognised by devman and enable various things
#
#	DEBUG_AHCI_INIT			Dump debug info about AHCI controller init
#	VERY_NOISY_AHCI_INIT	Dump **lots** of extra debug info about AHCI init
#
CDEFS=-DDEBUG_AHCI_INIT

include ../server_common.mk