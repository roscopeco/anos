SERVER_NAME?=pcidrv
BINARY?=$(SERVER_NAME)

BINARY_OBJS=main.o pci.o enumerate.o

# The following C defines are recognised by the PCI driver and enable various things
#
#	VERY_NOISY_BUS_DRIVER	Dump **lots** of info about ACPI tables
#	DEBUG_BUS_DRIVER_INIT	Dump info about the initialisation and ECAM mapping
#	DEBUG_BUS_DRIVER_ENUM	Dump info about PCI devices that are enumerated
#
CDEFS=

include ../server_common.mk
