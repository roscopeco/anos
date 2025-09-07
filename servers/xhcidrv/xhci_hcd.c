/*
 * xHCI Host Controller Driver Implementation
 * anos - An Operating System
 *
 * xHCI implementation of USB Core Host Controller Interface
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "../../common/usb/usb_core.h"
#include "../../common/usb/usb_spec.h"
#include "include/xhci_hcd.h"

#ifdef DEBUG_XHCI_HCD
#define hcd_debugf(...) printf(__VA_ARGS__)
#else
#define hcd_debugf(...)
#endif

// Forward declarations
int xhci_submit_control_transfer(XhciHostController *xhci_hcd,
                                 UsbTransfer *transfer);
int xhci_submit_bulk_transfer(XhciHostController *xhci_hcd,
                              UsbTransfer *transfer);
int xhci_submit_interrupt_transfer(XhciHostController *xhci_hcd,
                                   UsbTransfer *transfer);

// =============================================================================
// Host Controller Operations Table
// =============================================================================

static UsbHostControllerOps xhci_hcd_ops = {
        .submit_transfer = xhci_hcd_submit_transfer,
        .cancel_transfer = xhci_hcd_cancel_transfer,
        .reset_device = xhci_hcd_reset_device,
        .set_address = xhci_hcd_set_address,
        .enable_device = xhci_hcd_enable_device,
        .disable_device = xhci_hcd_disable_device,
        .configure_endpoint = xhci_hcd_configure_endpoint,
        .get_port_status = xhci_hcd_get_port_status,
        .reset_port = xhci_hcd_reset_port,
        .enable_port = xhci_hcd_enable_port,
        .disable_port = xhci_hcd_disable_port,
};

// =============================================================================
// Host Controller Setup
// =============================================================================

int xhci_hcd_init(XhciHostController *xhci_hcd, uint64_t base_addr,
                  uint64_t pci_config_base) {
    if (!xhci_hcd) {
        return -1;
    }

    memset(xhci_hcd, 0, sizeof(XhciHostController));

    // Initialize base xHCI controller
    if (!xhci_controller_init(&xhci_hcd->base, base_addr, pci_config_base)) {
        printf("xHCI HCD: Failed to initialize base controller\n");
        return -1;
    }

    // Initialize command ring
    if (xhci_ring_init(&xhci_hcd->command_ring, XHCI_RING_SIZE) != 0) {
        printf("xHCI HCD: Failed to initialize command ring\n");
        return -1;
    }

    // Initialize event ring
    if (xhci_ring_init(&xhci_hcd->event_ring, XHCI_RING_SIZE) != 0) {
        printf("xHCI HCD: Failed to initialize event ring\n");
        xhci_ring_free(&xhci_hcd->command_ring);
        return -1;
    }

    // Allocate and initialize DCBAA
    SyscallResultA dcbaa_alloc;
    dcbaa_alloc = anos_alloc_physical_pages(4096);
    if (dcbaa_alloc.result != SYSCALL_OK) {
        printf("xHCI HCD: Failed to allocate DCBAA - syscall result: "
               "0x%016lx\n",
               (uint64_t)dcbaa_alloc.result);
        printf("xHCI HCD: Requested 1 page (4096 bytes)\n");
        return -1;
    }

    xhci_hcd->dcbaa_physical = dcbaa_alloc.value;

    SyscallResult dcbaa_map;
    dcbaa_map = anos_map_physical(
            xhci_hcd->dcbaa_physical,
            (void *)(0x600000000 + xhci_hcd->dcbaa_physical), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (dcbaa_map.result != SYSCALL_OK) {
        printf("xHCI HCD: Failed to map DCBAA\n");
        return -1;
    }

    xhci_hcd->dcbaa = (XhciDcbaa *)(0x600000000 + xhci_hcd->dcbaa_physical);
    memset(xhci_hcd->dcbaa, 0, sizeof(XhciDcbaa));

    // Set DCBAAP register
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_DCBAAP,
                 (uint32_t)(xhci_hcd->dcbaa_physical & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_DCBAAP + 4,
                 (uint32_t)(xhci_hcd->dcbaa_physical >> 32));

    // Set command ring control register
    uint64_t crcr = xhci_hcd->command_ring.trbs_physical | 0x1; // RCS bit
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CRCR,
                 (uint32_t)(crcr & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CRCR + 4,
                 (uint32_t)(crcr >> 32));

    // Create USB host controller instance
    xhci_hcd->usb_hcd = malloc(sizeof(UsbHostController));
    if (!xhci_hcd->usb_hcd) {
        printf("xHCI HCD: Failed to allocate USB host controller\n");
        return -1;
    }

    memset(xhci_hcd->usb_hcd, 0, sizeof(UsbHostController));
    xhci_hcd->usb_hcd->name = "xHCI Host Controller";
    xhci_hcd->usb_hcd->ops = &xhci_hcd_ops;
    xhci_hcd->usb_hcd->max_devices = 127;
    xhci_hcd->usb_hcd->num_ports = xhci_hcd->base.max_ports;
    xhci_hcd->usb_hcd->supported_speeds =
            (1 << USB_SPEED_LOW) | (1 << USB_SPEED_FULL) |
            (1 << USB_SPEED_HIGH) | (1 << USB_SPEED_SUPER);
    xhci_hcd->usb_hcd->private_data = xhci_hcd;

    hcd_debugf(
            "xHCI HCD: Initialized with %u ports, command ring at 0x%016lx\n",
            xhci_hcd->usb_hcd->num_ports, xhci_hcd->command_ring.trbs_physical);

    return 0;
}

void xhci_hcd_shutdown(XhciHostController *xhci_hcd) {
    if (!xhci_hcd)
        return;

    // Clean up rings
    xhci_ring_free(&xhci_hcd->command_ring);
    xhci_ring_free(&xhci_hcd->event_ring);

    // Clean up USB host controller
    if (xhci_hcd->usb_hcd) {
        free(xhci_hcd->usb_hcd);
        xhci_hcd->usb_hcd = NULL;
    }

    // TODO: Free device contexts and other allocated memory

    memset(xhci_hcd, 0, sizeof(XhciHostController));
}

// =============================================================================
// USB Host Controller Operations Implementation
// =============================================================================

int xhci_hcd_submit_transfer(UsbHostController *hcd, UsbTransfer *transfer) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !transfer) {
        return -1;
    }

    hcd_debugf(
            "xHCI HCD: Submitting %s transfer to device %u endpoint 0x%02x\n",
            (transfer->type == USB_TRANSFER_CONTROL)     ? "control"
            : (transfer->type == USB_TRANSFER_BULK)      ? "bulk"
            : (transfer->type == USB_TRANSFER_INTERRUPT) ? "interrupt"
                                                         : "isochronous",
            transfer->device->address, transfer->endpoint);

    switch (transfer->type) {
    case USB_TRANSFER_CONTROL:
        return xhci_submit_control_transfer(xhci_hcd, transfer);
    case USB_TRANSFER_BULK:
        return xhci_submit_bulk_transfer(xhci_hcd, transfer);
    case USB_TRANSFER_INTERRUPT:
        return xhci_submit_interrupt_transfer(xhci_hcd, transfer);
    default:
        printf("xHCI HCD: Unsupported transfer type %d\n", transfer->type);
        return -1;
    }
}

int xhci_hcd_cancel_transfer(UsbHostController *hcd, UsbTransfer *transfer) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !transfer) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Cancelling transfer\n");

    // TODO: Implement transfer cancellation
    // Would need to send Stop Endpoint command and clean up transfer rings

    return 0;
}

int xhci_hcd_set_address(UsbHostController *hcd, UsbDevice *device,
                         uint8_t address) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !device) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Setting address %u for device on port %u\n", address,
               device->port_number);

    // Allocate device slot if not already done
    uint8_t slot_id = (uint8_t)(uintptr_t)device->hcd_private;
    if (slot_id == 0) {
        slot_id = xhci_alloc_device_slot(xhci_hcd);
        if (slot_id == 0) {
            printf("xHCI HCD: Failed to allocate device slot\n");
            return -1;
        }
        device->hcd_private = (void *)(uintptr_t)slot_id;
    }

    // Setup device context
    if (xhci_setup_device_context(xhci_hcd, slot_id, device) != 0) {
        printf("xHCI HCD: Failed to setup device context\n");
        return -1;
    }

    // Send Address Device command
    XhciAddressDeviceTrb address_cmd = {0};
    address_cmd.input_context_ptr = xhci_hcd->input_context_physical[slot_id];
    address_cmd.control = TRB_CONTROL_CYCLE_BIT |
                          (TRB_TYPE_ADDRESS_DEVICE << TRB_CONTROL_TYPE_SHIFT) |
                          (slot_id << TRB_CONTROL_SLOT_ID_SHIFT);

    int result = xhci_send_command(xhci_hcd, (XhciTrb *)&address_cmd);
    if (result != 0) {
        printf("xHCI HCD: Address Device command failed\n");
        return -1;
    }

    // Wait for command completion
    result = xhci_wait_for_command_completion(xhci_hcd, 5000);
    if (result != 0) {
        printf("xHCI HCD: Address Device command timed out\n");
        return -1;
    }

    hcd_debugf("xHCI HCD: Successfully set address %u\n", address);
    return 0;
}

int xhci_hcd_enable_device(UsbHostController *hcd, UsbDevice *device) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !device) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Enabling device on port %u\n", device->port_number);

    // Allocate device slot
    uint8_t slot_id = xhci_alloc_device_slot(xhci_hcd);
    if (slot_id == 0) {
        printf("xHCI HCD: Failed to allocate device slot\n");
        return -1;
    }

    device->hcd_private = (void *)(uintptr_t)slot_id;

    // Send Enable Slot command
    XhciEnableSlotTrb enable_cmd = {0};
    enable_cmd.control = TRB_CONTROL_CYCLE_BIT |
                         (TRB_TYPE_ENABLE_SLOT << TRB_CONTROL_TYPE_SHIFT);

    int result = xhci_send_command(xhci_hcd, (XhciTrb *)&enable_cmd);
    if (result != 0) {
        printf("xHCI HCD: Enable Slot command failed\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    result = xhci_wait_for_command_completion(xhci_hcd, 5000);
    if (result != 0) {
        printf("xHCI HCD: Enable Slot command timed out\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    hcd_debugf("xHCI HCD: Device enabled in slot %u\n", slot_id);
    return 0;
}

int xhci_hcd_disable_device(UsbHostController *hcd, UsbDevice *device) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !device) {
        return -1;
    }

    uint8_t slot_id = (uint8_t)(uintptr_t)device->hcd_private;
    if (slot_id == 0) {
        return 0; // Already disabled
    }

    hcd_debugf("xHCI HCD: Disabling device in slot %u\n", slot_id);

    // TODO: Send Disable Slot command
    xhci_free_device_slot(xhci_hcd, slot_id);
    device->hcd_private = NULL;

    return 0;
}

int xhci_hcd_reset_device(UsbHostController *hcd, UsbDevice *device) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !device) {
        return -1;
    }

    uint8_t slot_id = (uint8_t)(uintptr_t)device->hcd_private;
    hcd_debugf("xHCI HCD: Resetting device in slot %u (stub)\n", slot_id);
    (void)slot_id; // Suppress unused variable warning for non-debug builds

    // TODO: Send Reset Device command
    return 0;
}

// =============================================================================
// Port Management Operations
// =============================================================================

int xhci_hcd_get_port_status(UsbHostController *hcd, uint8_t port,
                             uint32_t *status) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || !status || port >= xhci_hcd->base.max_ports) {
        return -1;
    }

    const uint32_t port_offset = port * 0x10;
    *status =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.port_regs, port_offset);

    hcd_debugf("xHCI HCD: Port %u status: 0x%08x\n", port, *status);
    return 0;
}

int xhci_hcd_reset_port(UsbHostController *hcd, uint8_t port) {
    XhciHostController *xhci_hcd = xhci_hcd_from_usb_hcd(hcd);
    if (!xhci_hcd || port >= xhci_hcd->base.max_ports) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Resetting port %u\n", port);

    const uint32_t port_offset = port * 0x10;
    uint32_t portsc =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.port_regs, port_offset);

    // Set port reset bit
    portsc |= XHCI_PORTSC_PR;
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.port_regs, port_offset,
                 portsc);

    // TODO: Wait for reset completion

    return 0;
}

int xhci_hcd_enable_port(UsbHostController *hcd, uint8_t port) {
    hcd_debugf("xHCI HCD: Port enable not needed for xHCI (auto-enabled)\n");
    return 0; // xHCI ports are automatically enabled when device is connected
}

int xhci_hcd_disable_port(UsbHostController *hcd, uint8_t port) {
    hcd_debugf("xHCI HCD: Disabling port %u\n", port);
    // TODO: Implement port disable if needed
    return 0;
}

// =============================================================================
// Utility Functions
// =============================================================================

XhciHostController *xhci_hcd_from_usb_hcd(UsbHostController *hcd) {
    if (!hcd || !hcd->private_data) {
        return NULL;
    }
    return (XhciHostController *)hcd->private_data;
}

// =============================================================================
// Stub Functions (To Be Implemented)
// =============================================================================

int xhci_hcd_configure_endpoint(UsbHostController *hcd, UsbDevice *device,
                                UsbEndpointDescriptor *ep_desc) {
    hcd_debugf("xHCI HCD: Configure endpoint (stub)\n");
    return 0; // TODO: Implement endpoint configuration
}

int xhci_send_command(XhciHostController *xhci_hcd, XhciTrb *command_trb) {
    hcd_debugf("xHCI HCD: Send command (stub)\n");
    return 0; // TODO: Implement command sending
}

int xhci_wait_for_command_completion(XhciHostController *xhci_hcd,
                                     uint32_t timeout_ms) {
    hcd_debugf("xHCI HCD: Wait for command completion (stub)\n");
    return 0; // TODO: Implement command completion waiting
}

int xhci_alloc_device_slot(XhciHostController *xhci_hcd) {
    for (uint8_t i = 1; i < XHCI_MAX_DEVICES; i++) {
        if (!xhci_hcd->slots_enabled[i]) {
            xhci_hcd->slots_enabled[i] = true;
            hcd_debugf("xHCI HCD: Allocated slot %u\n", i);
            return i;
        }
    }
    return 0; // No slots available
}

void xhci_free_device_slot(XhciHostController *xhci_hcd, uint8_t slot_id) {
    if (slot_id > 0 && slot_id < XHCI_MAX_DEVICES) {
        xhci_hcd->slots_enabled[slot_id] = false;
        hcd_debugf("xHCI HCD: Freed slot %u\n", slot_id);
    }
}

int xhci_setup_device_context(XhciHostController *xhci_hcd, uint8_t slot_id,
                              UsbDevice *device) {
    hcd_debugf("xHCI HCD: Setup device context for slot %u (stub)\n", slot_id);
    return 0; // TODO: Implement device context setup
}

int xhci_submit_control_transfer(XhciHostController *xhci_hcd,
                                 UsbTransfer *transfer) {
    hcd_debugf("xHCI HCD: Submit control transfer (stub)\n");
    // For now, mark transfer as completed immediately
    transfer->status = USB_TRANSFER_STATUS_COMPLETED;
    transfer->actual_length = transfer->buffer_length;
    return 0;
}

int xhci_submit_bulk_transfer(XhciHostController *xhci_hcd,
                              UsbTransfer *transfer) {
    hcd_debugf("xHCI HCD: Submit bulk transfer (stub)\n");
    transfer->status = USB_TRANSFER_STATUS_COMPLETED;
    return 0;
}

int xhci_submit_interrupt_transfer(XhciHostController *xhci_hcd,
                                   UsbTransfer *transfer) {
    hcd_debugf("xHCI HCD: Submit interrupt transfer (stub)\n");
    transfer->status = USB_TRANSFER_STATUS_COMPLETED;
    return 0;
}