USB_COMMON_NAME=usbcore
BINARY=$(USB_COMMON_NAME)
BINARY_OBJS=usb_core.o

# USB core library build
CDEFS=

include ../../server_common.mk

# Override default target to create a library instead of executable
$(BINARY_NAME): $(BINARY_OBJS)
	$(XAR) rcs lib$(USB_COMMON_NAME).a $(BINARY_OBJS)
	
clean:
	rm -f *.o *.a *.elf