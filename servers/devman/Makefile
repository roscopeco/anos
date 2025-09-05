SERVER_NAME?=devman
BINARY?=$(SERVER_NAME)

BINARY_OBJS=main.o

# The following C defines are recognised by devman and enable various things
#
#	DEBUG_ACPI				Dump info about ACPI tables
#	VERY_NOISY_ACPI			Dump **lots** of info about ACPI tables
#	DEBUG_PCI				Dump info about PCI drivers that are spawned
#	DEBUG_PCI_MCFG			Dump info about the MCFG ACPI table
#	DEBUG_DEVICE_REG		Dump info about device registration
#	DEBUG_DEVICE			Dump info about device communication
#
CDEFS=

include ../server_common.mk
