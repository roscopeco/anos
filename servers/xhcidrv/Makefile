SERVER_NAME=xhcidrv
BINARY=xhcidrv
BINARY_OBJS=main.o xhci.o pci.o xhci_rings.o xhci_hcd.o

# Link with USB core library
USB_CORE_DIR=../common/usb
LDFLAGS=-L$(USB_CORE_DIR) -lusbcore

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

# Build USB core library first
$(USB_CORE_DIR)/libusbcore.a:
	$(MAKE) -C $(USB_CORE_DIR)

# Override linking to include USB core library
$(BINARY_NAME): $(BINARY_OBJS) $(USB_CORE_DIR)/libusbcore.a
	$(XCC) -o $@ $(BINARY_OBJS) $(LDFLAGS)
	chmod a-x $@