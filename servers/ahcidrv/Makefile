SERVER_NAME=ahcidrv
BINARY=ahcidrv
BINARY_OBJS=main.o ahci.o pci.o

# The following C defines are recognised by devman and enable various things
#
#	DEBUG_AHCI_INIT			Dump debug info about AHCI controller init
#	VERY_NOISY_AHCI_INIT	Dump **lots** of extra debug info about AHCI init
#	DEBUG_AHCI_PCI			Dump debug info from the driver's PCI interface
#	DEBUG_AHCI_OPS			Dump debug info about the drivers device operations
#	VERY_NOISY_AHCI_OPS		Dump **lots** of extra debug info about AHCI device ops
#
CDEFS=

include ../server_common.mk