SERVER_NAME=xhcidrv
BINARY=xhcidrv
BINARY_OBJS=main.o xhci.o pci.o

# The following C defines are recognised by the xHCI driver and enable various things
#
#	DEBUG_XHCI_INIT			Dump debug info about xHCI controller init
#	VERY_NOISY_XHCI_INIT	Dump **lots** of extra debug info about xHCI init
#	DEBUG_XHCI_PCI			Dump debug info from the driver's PCI interface
#	DEBUG_XHCI_OPS			Dump debug info about the drivers device operations
#	VERY_NOISY_XHCI_OPS		Dump **lots** of extra debug info about xHCI device ops
#
CDEFS=

include ../server_common.mk