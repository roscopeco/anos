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
#ifdef VERY_NOISY_XHCI_HCD
#define hcd_vdebugf(...) printf(__VA_ARGS__)
#else
#define hcd_vdebugf(...)
#endif
#else
#define hcd_debugf(...)
#define hcd_vdebugf(...)
#endif

// Forward declarations
int xhci_submit_control_transfer(XhciHostController *xhci_hcd,
                                 UsbTransfer *transfer);
int xhci_submit_bulk_transfer(XhciHostController *xhci_hcd,
                              UsbTransfer *transfer);
int xhci_submit_interrupt_transfer(XhciHostController *xhci_hcd,
                                   UsbTransfer *transfer);
int xhci_build_control_transfer_trbs(XhciHostController *xhci_hcd,
                                     UsbTransfer *transfer, uint8_t slot_id);
void xhci_ring_doorbell(XhciHostController *xhci_hcd, uint8_t slot_id,
                        uint8_t target);
int xhci_setup_event_ring(XhciHostController *xhci_hcd);

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
        hcd_debugf("xHCI HCD: Failed to initialize base controller\n");
        return -1;
    }

    // Initialize command ring
    if (xhci_ring_init(&xhci_hcd->command_ring, XHCI_RING_SIZE) != 0) {
        hcd_debugf("xHCI HCD: Failed to initialize command ring\n");
        return -1;
    }

    // Initialize event ring
    if (xhci_ring_init(&xhci_hcd->event_ring, XHCI_RING_SIZE) != 0) {
        hcd_debugf("xHCI HCD: Failed to initialize event ring\n");
        xhci_ring_free(&xhci_hcd->command_ring);
        return -1;
    }

    // Allocate and initialize DCBAA
    const SyscallResultA dcbaa_alloc = anos_alloc_physical_pages(4096);
    if (dcbaa_alloc.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to allocate DCBAA - syscall result: "
                   "0x%016lx\n",
                   (uint64_t)dcbaa_alloc.result);
        hcd_debugf("xHCI HCD: Requested 1 page (4096 bytes)\n");
        return -1;
    }

    xhci_hcd->dcbaa_physical = dcbaa_alloc.value;

    const SyscallResult dcbaa_map = anos_map_physical(
            xhci_hcd->dcbaa_physical,
            (void *)(0x600000000 + xhci_hcd->dcbaa_physical), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (dcbaa_map.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to map DCBAA\n");
        return -1;
    }

    xhci_hcd->dcbaa =
            (volatile XhciDcbaa *)(0x600000000 + xhci_hcd->dcbaa_physical);
    memset((void *)xhci_hcd->dcbaa, 0, sizeof(XhciDcbaa));

    // Set DCBAAP register
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_DCBAAP,
                 (uint32_t)(xhci_hcd->dcbaa_physical & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_DCBAAP + 4,
                 (uint32_t)(xhci_hcd->dcbaa_physical >> 32));

    // CRITICAL: Set CONFIG register with maximum device slots
    // This MUST be set before any Enable Slot commands will work
    uint32_t max_device_slots = xhci_hcd->base.max_slots;
    if (max_device_slots > XHCI_MAX_DEVICES) {
        max_device_slots = XHCI_MAX_DEVICES;
    }
    hcd_vdebugf("xHCI HCD: Setting CONFIG register - MaxDeviceSlots=%u\n",
                max_device_slots);
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CONFIG,
                 max_device_slots);

#ifdef DEBUG_XHCI_HCD
    const uint32_t config_readback = xhci_read32(
            &xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CONFIG);
    hcd_vdebugf("xHCI HCD: CONFIG register readback: 0x%08x (should be %u)\n",
                config_readback, max_device_slots);
#endif

    const uint32_t hccparams1 = xhci_read32(
            &xhci_hcd->base, xhci_hcd->base.cap_regs, XHCI_CAP_HCCPARAMS1);
#ifdef DEBUG_XHCI_HCD
    uint32_t hccparams2 = xhci_read32(&xhci_hcd->base, xhci_hcd->base.cap_regs,
                                      XHCI_CAP_HCCPARAMS2);
    uint32_t hcsparams2 = xhci_read32(&xhci_hcd->base, xhci_hcd->base.cap_regs,
                                      XHCI_CAP_HCSPARAMS2);

    printf("xHCI HCD: Capability Parameters Debug:\n");
    printf("  HCCPARAMS1: 0x%08x\n", hccparams1);
    printf("    64-bit addressing: %s\n", (hccparams1 & 0x1) ? "YES" : "NO");
    printf("    Bandwidth negotiation: %s\n",
           (hccparams1 & 0x2) ? "YES" : "NO");
    printf("    Context size: %s\n",
           (hccparams1 & 0x4) ? "64-byte" : "32-byte");
    printf("    Port power control: %s\n", (hccparams1 & 0x8) ? "YES" : "NO");
    printf("    Port indicators: %s\n", (hccparams1 & 0x10) ? "YES" : "NO");
    printf("    Light HC reset: %s\n", (hccparams1 & 0x20) ? "YES" : "NO");
    printf("  HCCPARAMS2: 0x%08x\n", hccparams2);
    printf("  HCSPARAMS2: 0x%08x\n", hcsparams2);
#endif

    // Check extended capabilities pointer
    const uint16_t xecp = (hccparams1 >> 16) & 0xFFFF;
    hcd_vdebugf("  Extended capabilities pointer: 0x%04x\n", xecp);
    if (xecp != 0) {
        hcd_vdebugf(
                "  WARNING: Controller has extended capabilities that may need "
                "setup\n");

        // Read first extended capability
        const uint32_t ext_cap =
                xhci_read32(&xhci_hcd->base, xhci_hcd->base.cap_regs, xecp);
        uint8_t cap_id = ext_cap & 0xFF;

#ifdef DEBUG_XHCI_HCD
#ifdef VERY_NOISY_XHCI_HCD
        uint8_t next_ptr = (ext_cap >> 8) & 0xFF;
        uint16_t cap_specific = (ext_cap >> 16) & 0xFFFF;

        hcd_vdebugf("  First extended capability: ID=0x%02x, Next=0x%02x, "
                    "Specific=0x%04x\n",
                    cap_id, next_ptr, cap_specific);
#endif
#endif

        if (cap_id == 1) { // USB Legacy Support Capability
            hcd_debugf("  Found USB Legacy Support Capability - may need to "
                       "disable OS handoff\n");

            // Read USB Legacy Support Control/Status
            if (xecp + 4 < 0x1000) { // Safety check
                uint32_t usblegctlsts = xhci_read32(
                        &xhci_hcd->base, xhci_hcd->base.cap_regs, xecp + 4);
                hcd_vdebugf("  USBLEGCTLSTS: 0x%08x\n", usblegctlsts);

                // Check if BIOS/SMM owns the controller
                if (usblegctlsts & (1 << 16)) { // HC BIOS Owned Semaphore
                    hcd_debugf("  WARNING: BIOS still owns the controller - "
                               "attempting OS handoff\n");

                    // Set OS Owned Semaphore
                    xhci_write32(&xhci_hcd->base, xhci_hcd->base.cap_regs,
                                 xecp + 4, usblegctlsts | (1 << 24));

                    // Wait for handoff (timeout after 1 second)
                    for (int timeout = 1000; timeout > 0; timeout--) {
                        usblegctlsts =
                                xhci_read32(&xhci_hcd->base,
                                            xhci_hcd->base.cap_regs, xecp + 4);
                        if (!(usblegctlsts &
                              (1 << 16))) { // BIOS released control
                            hcd_debugf("  OS handoff successful\n");
                            break;
                        }
                        // Small delay
                        for (volatile int i = 0; i < 10000; i++)
                            ;
                    }

                    if (usblegctlsts & (1 << 16)) {
                        printf("  WARNING: OS handoff failed - BIOS still owns "
                               "controller\n");
                    }
                }
            }
        }
    }

    // Set command ring control register
    uint64_t crcr = xhci_hcd->command_ring.trbs_physical | 0x1; // RCS bit
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CRCR,
                 (uint32_t)(crcr & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CRCR + 4,
                 (uint32_t)(crcr >> 32));

    // Set up Event Ring Segment Table (ERST)
    if (xhci_setup_event_ring(xhci_hcd) != 0) {
        hcd_debugf("xHCI HCD: Failed to setup event ring\n");
        return -1;
    }

    // Create USB host controller instance
    xhci_hcd->usb_hcd = malloc(sizeof(UsbHostController));
    if (!xhci_hcd->usb_hcd) {
        hcd_debugf("xHCI HCD: Failed to allocate USB host controller\n");
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
        hcd_debugf("xHCI HCD: Unsupported transfer type %d\n", transfer->type);
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
            hcd_debugf("xHCI HCD: Failed to allocate device slot\n");
            return -1;
        }
        device->hcd_private = (void *)(uintptr_t)slot_id;
    }

    // Setup device context
    if (xhci_setup_device_context(xhci_hcd, slot_id, device) != 0) {
        hcd_debugf("xHCI HCD: Failed to setup device context\n");
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
        hcd_debugf("xHCI HCD: Address Device command failed\n");
        return -1;
    }

    // Wait for command completion
    result = xhci_wait_for_command_completion(xhci_hcd, 5000);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Address Device command timed out\n");
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
    const uint8_t slot_id = xhci_alloc_device_slot(xhci_hcd);
    if (slot_id == 0) {
        hcd_debugf("xHCI HCD: Failed to allocate device slot\n");
        return -1;
    }

    device->hcd_private = (void *)(uintptr_t)slot_id;

    // Send Enable Slot command
    XhciEnableSlotTrb enable_cmd = {0};
    enable_cmd.control = TRB_CONTROL_CYCLE_BIT |
                         (TRB_TYPE_ENABLE_SLOT << TRB_CONTROL_TYPE_SHIFT);

    hcd_debugf("xHCI HCD: Sending Enable Slot TRB: reserved1=0x%016lx "
               "reserved2=0x%08x control=0x%08x\n",
               enable_cmd.reserved1, enable_cmd.reserved2, enable_cmd.control);

    int result = xhci_send_command(xhci_hcd, (XhciTrb *)&enable_cmd);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Enable Slot command failed\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    result = xhci_wait_for_command_completion(xhci_hcd, 5000);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Enable Slot command timed out\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    hcd_debugf("xHCI HCD: Enable Slot completed for slot %u, setting up device "
               "context\n",
               slot_id);

    // Set up device context and endpoint 0 after successful Enable Slot
    result = xhci_setup_device_context(xhci_hcd, slot_id, device);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Failed to setup device context for slot %u\n",
                   slot_id);
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    // Send Address Device command to configure endpoint 0
    XhciAddressDeviceTrb address_cmd = {0};
    address_cmd.input_context_ptr = xhci_hcd->input_context_physical[slot_id];
    address_cmd.control = TRB_CONTROL_CYCLE_BIT |
                          (TRB_TYPE_ADDRESS_DEVICE << TRB_CONTROL_TYPE_SHIFT) |
                          (slot_id << TRB_CONTROL_SLOT_ID_SHIFT);

    hcd_debugf("xHCI HCD: Sending Address Device command for slot %u\n",
               slot_id);
    result = xhci_send_command(xhci_hcd, (XhciTrb *)&address_cmd);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Address Device command failed\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    result = xhci_wait_for_command_completion(xhci_hcd, 5000);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Address Device command timed out\n");
        xhci_free_device_slot(xhci_hcd, slot_id);
        return -1;
    }

    hcd_debugf("xHCI HCD: Device fully enabled in slot %u with endpoint 0 "
               "configured\n",
               slot_id);
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
    if (!xhci_hcd || !command_trb) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Sending command TRB type %u\n",
               TRB_GET_TYPE(command_trb));

    // Verify controller is running before sending command
#ifdef DEBUG_XHCI_HCD
    uint32_t usbsts = xhci_read32(&xhci_hcd->base, xhci_hcd->base.op_regs,
                                  XHCI_OP_USBSTS);
    uint32_t usbcmd = xhci_read32(&xhci_hcd->base, xhci_hcd->base.op_regs,
                                  XHCI_OP_USBCMD);
    hcd_vdebugf("xHCI HCD: Controller status before command - USBSTS=0x%08x "
                "USBCMD=0x%08x\n",
                usbsts, usbcmd);
    hcd_vdebugf("  HCH (Halted): %s, CNR (Not Ready): %s, Run bit: %s\n",
                (usbsts & XHCI_STS_HCH) ? "YES" : "NO",
                (usbsts & XHCI_STS_CNR) ? "YES" : "NO",
                (usbcmd & XHCI_CMD_RUN) ? "YES" : "NO");

    hcd_vdebugf("  Full USBSTS: 0x%08x\n", usbsts);
    hcd_vdebugf("    HSE (Host System Error): %s\n",
                (usbsts & (1 << 2)) ? "YES" : "NO");
    hcd_vdebugf("    EINT (Event Interrupt): %s\n",
                (usbsts & (1 << 3)) ? "YES" : "NO");
    hcd_vdebugf("    PCD (Port Change Detect): %s\n",
                (usbsts & (1 << 4)) ? "YES" : "NO");
    hcd_vdebugf("    SSS (Save State Status): %s\n",
                (usbsts & (1 << 8)) ? "YES" : "NO");
    hcd_vdebugf("    RSS (Restore State Status): %s\n",
                (usbsts & (1 << 9)) ? "YES" : "NO");
    hcd_vdebugf("    SRE (Save/Restore Error): %s\n",
                (usbsts & (1 << 10)) ? "YES" : "NO");
    hcd_vdebugf("    CAS (Command Abort Status): %s\n",
                (usbsts & (1 << 11)) ? "YES" : "NO");

    // Check DNCTRL register
    uint32_t dnctrl = xhci_read32(&xhci_hcd->base, xhci_hcd->base.op_regs,
                                  XHCI_OP_DNCTRL);
    hcd_debugf("  DNCTRL (Device Notification Control): 0x%08x\n", dnctrl);
#endif

    // Enqueue the command TRB to the command ring
    XhciTrb *ring_trb = xhci_ring_enqueue_trb(&xhci_hcd->command_ring);
    if (!ring_trb) {
        hcd_debugf("xHCI HCD: Failed to enqueue command TRB\n");
        return -1;
    }

    // Copy the command TRB to the ring
    *ring_trb = *command_trb;

    // CRITICAL: Ensure TRB write reaches memory before doorbell ring
    // Memory barrier to ensure TRB is fully written to memory before doorbell
    __asm__ volatile("mfence" ::: "memory");

    // DEBUG: Verify TRB was actually written to memory
    hcd_vdebugf("xHCI HCD: TRB written to ring[%u] - verifying...\n",
                xhci_hcd->command_ring.enqueue_index);
    hcd_vdebugf("  Written TRB: param=0x%016lx status=0x%08x control=0x%08x\n",
                command_trb->parameter, command_trb->status,
                command_trb->control);
    hcd_vdebugf("  Ring TRB:    param=0x%016lx status=0x%08x control=0x%08x\n",
                ring_trb->parameter, ring_trb->status, ring_trb->control);
    hcd_debugf("  Ring TRB address: %lx (physical: 0x%016lx + %u * %lu = "
               "0x%016lx)\n",
               (uintptr_t)ring_trb, xhci_hcd->command_ring.trbs_physical,
               xhci_hcd->command_ring.enqueue_index, sizeof(XhciTrb),
               xhci_hcd->command_ring.trbs_physical +
                       xhci_hcd->command_ring.enqueue_index * sizeof(XhciTrb));

    // Advance the enqueue pointer
    xhci_ring_inc_enqueue(&xhci_hcd->command_ring);
#ifdef DEBUG_XHCI_HCD
    uint32_t old_enqueue = xhci_hcd->command_ring.enqueue_index;
    hcd_debugf("xHCI HCD: Command ring - enqueue advanced from %u to %u, "
               "cycle=%u\n",
               old_enqueue, xhci_hcd->command_ring.enqueue_index,
               xhci_hcd->command_ring.producer_cycle_state);
#endif

    // Ring the host controller doorbell (slot 0 = command ring)
    hcd_debugf(
            "xHCI HCD: Ringing doorbell - doorbell_regs=%p, writing 0x00000000 "
            "to offset 0\n",
            xhci_hcd->base.doorbell_regs);

    xhci_write32(&xhci_hcd->base, xhci_hcd->base.doorbell_regs, 0, 0);

    // Ensure doorbell write completes
    __asm__ volatile("mfence" ::: "memory");

#ifdef DEBUG_XHCI_HCD
#ifdef VERY_NOISY_XHCI_HCD
    // DEBUG: Try manual interrupt trigger to test event mechanism
    printf("xHCI HCD: Testing interrupt mechanism...\n");
    uint32_t iman =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs, 0x20);
    printf("  IMAN (Interrupt Management): 0x%08x\n", iman);

    // Set Interrupt Pending bit manually to test if events work
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs, 0x20,
                 iman | (1 << 0));

    uint32_t iman_after =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs, 0x20);
    printf("  IMAN after setting IP bit: 0x%08x\n", iman_after);
#endif

    // Verify doorbell write (though doorbell registers are typically write-only)
    uint32_t doorbell_readback =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.doorbell_regs, 0);
    printf("xHCI HCD: Doorbell readback: 0x%08x (may be 0 if write-only)\n",
           doorbell_readback);

    hcd_debugf("xHCI HCD: Command TRB enqueued and doorbell rung\n");
#endif
    return 0;
}

int xhci_wait_for_command_completion(XhciHostController *xhci_hcd,
                                     const uint32_t timeout_ms) {
    if (!xhci_hcd) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Waiting for command completion (timeout %u ms)\n",
               timeout_ms);

    // For now, implement a simple polling mechanism
    // In a real implementation, this would process the event ring
    hcd_vdebugf(
            "xHCI HCD: Starting command completion wait, event ring @ %p (phys "
            "0x%016lx)\n",
            (void *)xhci_hcd->event_ring.trbs,
            xhci_hcd->event_ring.trbs_physical);
    hcd_vdebugf("xHCI HCD: Event ring enqueue=%u dequeue=%u cycle=%u\n",
                xhci_hcd->event_ring.enqueue_index,
                xhci_hcd->event_ring.dequeue_index,
                xhci_hcd->event_ring.consumer_cycle_state);

#ifdef DEBUG_XHCI_HCD_RINGS
    printf("xHCI HCD: EINT flag indicates events present - scanning entire "
           "event ring:\n");
    for (uint32_t scan_idx = 0; scan_idx < xhci_hcd->event_ring.size;
         scan_idx++) {
        volatile const XhciTrb *scan_trb = &xhci_hcd->event_ring.trbs[scan_idx];
        uint32_t control = scan_trb->control;
        if (control != 0) { // Non-zero TRB found
            uint32_t trb_type = TRB_GET_TYPE(scan_trb);
            bool cycle = (control & TRB_CONTROL_CYCLE_BIT) != 0;
            printf("  Ring[%u]: param=0x%016lx status=0x%08x control=0x%08x "
                   "(type=%u cycle=%u)\n",
                   scan_idx, scan_trb->parameter, scan_trb->status, control,
                   trb_type, cycle);
        }
    }
#endif

    for (uint32_t i = 0; i < timeout_ms; i++) {
        // Check for events manually since hardware controls enqueue for event ring
        volatile const XhciTrb *event_trb =
                &xhci_hcd->event_ring.trbs[xhci_hcd->event_ring.dequeue_index];
        bool trb_cycle = (event_trb->control & TRB_CONTROL_CYCLE_BIT) != 0;
        bool has_event =
                (event_trb->control != 0) &&
                (trb_cycle == xhci_hcd->event_ring.consumer_cycle_state);

        if (has_event) {
            const uint32_t trb_type = TRB_GET_TYPE(event_trb);
            hcd_debugf(
                    "xHCI HCD: Found event TRB type %u at index %u (cycle=%u "
                    "expected=%u)\n",
                    trb_type, xhci_hcd->event_ring.dequeue_index, trb_cycle,
                    xhci_hcd->event_ring.consumer_cycle_state);

            if (trb_type == TRB_TYPE_COMMAND_COMPLETION) {
                hcd_debugf("xHCI HCD: Command completed successfully\n");
                xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
                return 0;
            }
            xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
#ifdef DEBUG_XHCI_HCD
        } else {
            // Show first few attempts to see if we're polling correctly
            if (i < 10) {
                printf("xHCI HCD: No event at attempt %u (dequeue=%u "
                       "cycle=%u)\n",
                       i, xhci_hcd->event_ring.dequeue_index,
                       xhci_hcd->event_ring.consumer_cycle_state);
            }
#endif
        }

        // Sleep for 1ms
        anos_task_sleep_current(1000000);
    }

    hcd_debugf("xHCI HCD: Command completion timeout after %u ms\n",
               timeout_ms);
    return -1;
}

int xhci_wait_for_transfer_completion(XhciHostController *xhci_hcd,
                                      const uint32_t timeout_ms) {
    if (!xhci_hcd) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Waiting for transfer completion (timeout %u ms)\n",
               timeout_ms);

#ifdef DEBUG_XHCI_HCD_RINGS
    printf("xHCI HCD: Scanning entire event ring for transfer events:\n");
    for (uint32_t scan_idx = 0; scan_idx < xhci_hcd->event_ring.size;
         scan_idx++) {
        volatile const XhciTrb *scan_trb = &xhci_hcd->event_ring.trbs[scan_idx];
        uint32_t control = scan_trb->control;
        if (control != 0) { // Non-zero TRB found
            uint32_t trb_type = TRB_GET_TYPE(scan_trb);
            bool cycle = (control & TRB_CONTROL_CYCLE_BIT) != 0;
            printf("  Ring[%u]: param=0x%016lx status=0x%08x "
                   "control=0x%08x (type=%u cycle=%u)\n",
                   scan_idx, scan_trb->parameter, scan_trb->status, control,
                   trb_type, cycle);

            if (trb_type == TRB_TYPE_TRANSFER) {
                printf("  *** TRANSFER EVENT FOUND at index %u! ***\n",
                       scan_idx);
            }
        }
    }
#endif

    // Process all pending events in ring order until we find a transfer event
    hcd_debugf("xHCI HCD: Processing events sequentially from dequeue index %u "
               "(cycle=%u)\n",
               xhci_hcd->event_ring.dequeue_index,
               xhci_hcd->event_ring.consumer_cycle_state);

    for (uint32_t attempts = 0; attempts < timeout_ms; attempts++) {
        // Process all available events at current dequeue position
        while (true) {
            volatile const XhciTrb *event_trb =
                    &xhci_hcd->event_ring
                             .trbs[xhci_hcd->event_ring.dequeue_index];
            bool trb_cycle = (event_trb->control & TRB_CONTROL_CYCLE_BIT) != 0;
            bool has_event =
                    (event_trb->control != 0) &&
                    (trb_cycle == xhci_hcd->event_ring.consumer_cycle_state);

            if (!has_event) {
                break; // No more events at current position
            }

            const uint32_t trb_type = TRB_GET_TYPE(event_trb);
            hcd_vdebugf("xHCI HCD: Processing event TRB type %u at index %u "
                        "(cycle=%u)\n",
                        trb_type, xhci_hcd->event_ring.dequeue_index,
                        trb_cycle);

            if (trb_type == TRB_TYPE_TRANSFER) {
                hcd_debugf("xHCI HCD: Transfer event found - param=0x%016lx "
                           "status=0x%08x control=0x%08x\n",
                           event_trb->parameter, event_trb->status,
                           event_trb->control);

                // Check completion code
                const uint32_t completion_code =
                        (event_trb->status >> 24) & 0xFF;
                hcd_debugf("xHCI HCD: Transfer completion code: %u (%s)\n",
                           completion_code,
                           xhci_completion_code_string(completion_code));

                if (completion_code == XHCI_COMP_SUCCESS) {
                    hcd_debugf("xHCI HCD: Transfer completed successfully!\n");
                    xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
                    return 0;
                } else {
                    hcd_debugf("xHCI HCD: Transfer failed with completion code "
                               "%u\n",
                               completion_code);
                    xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
                    return -1;
                }
            } else if (trb_type == TRB_TYPE_COMMAND_COMPLETION) {
                hcd_vdebugf("xHCI HCD: Skipping command completion event "
                            "during transfer wait\n");
                xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
            } else {
                hcd_vdebugf("xHCI HCD: Skipping unknown event type %u\n",
                            trb_type);
                xhci_ring_inc_dequeue(&xhci_hcd->event_ring);
            }
        }

        // Sleep for 1ms before checking for new events
        anos_task_sleep_current(1000000);
    }

    hcd_debugf("xHCI HCD: Transfer completion timeout after %u ms\n",
               timeout_ms);
    return -1;
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

int xhci_setup_endpoint_ring(XhciHostController *xhci_hcd, uint8_t slot_id,
                             uint8_t endpoint_id) {
    if (!xhci_hcd || slot_id == 0 || slot_id >= XHCI_MAX_DEVICES ||
        endpoint_id >= 32) {
        return -1;
    }

    // Allocate and initialize the endpoint transfer ring
    xhci_hcd->endpoint_rings[slot_id][endpoint_id] = malloc(sizeof(XhciRing));
    if (!xhci_hcd->endpoint_rings[slot_id][endpoint_id]) {
        hcd_debugf("xHCI HCD: Failed to allocate endpoint ring\n");
        return -1;
    }

    XhciRing *ring = xhci_hcd->endpoint_rings[slot_id][endpoint_id];
    if (xhci_ring_init(ring, XHCI_RING_SIZE) != 0) {
        hcd_debugf("xHCI HCD: Failed to initialize endpoint ring\n");
        free(ring);
        xhci_hcd->endpoint_rings[slot_id][endpoint_id] = NULL;
        return -1;
    }

    hcd_debugf("xHCI HCD: Initialized endpoint ring for slot %u endpoint %u\n",
               slot_id, endpoint_id);
    return 0;
}

int xhci_setup_device_context(XhciHostController *xhci_hcd, uint8_t slot_id,
                              UsbDevice *device) {
    hcd_debugf("xHCI HCD: Setting up device context for slot %u\n", slot_id);

    if (!xhci_hcd || slot_id == 0 || slot_id >= XHCI_MAX_DEVICES || !device) {
        return -1;
    }

    // Allocate input context (1 page should be enough for slot + ep0 context)
    const SyscallResultA input_ctx_alloc = anos_alloc_physical_pages(4096);
    if (input_ctx_alloc.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to allocate input context\n");
        return -1;
    }

    // Map input context
    SyscallResult input_ctx_map = anos_map_physical(
            input_ctx_alloc.value,
            (void *)(0x900000000 + input_ctx_alloc.value), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (input_ctx_map.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to map input context\n");
        return -1;
    }

    volatile uint8_t *input_context =
            (volatile uint8_t *)(0x900000000 + input_ctx_alloc.value);
    memset((void *)input_context, 0, 4096);

    // Store the input context physical address
    xhci_hcd->input_context_physical[slot_id] = input_ctx_alloc.value;

    // Set up Input Control Context (first 32 bytes)
    volatile uint32_t *input_ctrl = (volatile uint32_t *)input_context;
    input_ctrl[1] =
            0x3; // A0 and A1 bits - slot context and EP0 context are affected

    // Set up Slot Context (starts at offset 32)
    volatile uint32_t *slot_context = (volatile uint32_t *)(input_context + 32);

    // Slot Context DW0: Route String and Speed
    slot_context[0] = (device->speed << 20); // Speed in bits 23:20

    // Slot Context DW1: Root Hub Port Number and Context Entries
    slot_context[1] = ((device->port_number + 1) << 16) |
                      1; // Port num (1-based) and 1 context entry (EP0)

    // Set up Endpoint 0 Context (starts at offset 64)
    volatile uint32_t *ep0_context = (volatile uint32_t *)(input_context + 64);

    // EP0 Context DW0: Endpoint State and Type
    ep0_context[0] = (4 << 3); // EP Type = Control (4), State = Disabled (0)

    // EP0 Context DW1: Max Packet Size
    uint16_t max_packet_size =
            64; // Default, should be updated after getting device descriptor
    switch (device->speed) {
    case 1:
        max_packet_size = 8;
        break; // Low speed
    case 2:
        max_packet_size = 64;
        break; // Full speed
    case 3:
        max_packet_size = 64;
        break; // High speed
    case 4:
        max_packet_size = 512;
        break; // Super speed
    }
    ep0_context[1] = (max_packet_size << 16) |
                     (3 << 1); // Max packet size and Error Count = 3

    // Set up endpoint 0 transfer ring
    if (xhci_setup_endpoint_ring(xhci_hcd, slot_id, 0) != 0) {
        hcd_debugf("xHCI HCD: Failed to setup endpoint 0 ring\n");
        return -1;
    }

    // Set Transfer Ring Base Address (TRBA) in endpoint context - DW2/DW3
    XhciRing *ep0_ring = xhci_hcd->endpoint_rings[slot_id][0];
    ep0_context[2] =
            (uint32_t)(ep0_ring->trbs_physical & 0xFFFFFFFF) | 1; // DCS = 1
    ep0_context[3] = (uint32_t)(ep0_ring->trbs_physical >> 32);

    // Allocate actual device context (separate from input context)
    const SyscallResultA device_ctx_alloc = anos_alloc_physical_pages(4096);
    if (device_ctx_alloc.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to allocate device context\n");
        return -1;
    }

    // Map device context
    SyscallResult device_ctx_map = anos_map_physical(
            device_ctx_alloc.value,
            (void *)(0xB00000000 + device_ctx_alloc.value), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (device_ctx_map.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to map device context\n");
        return -1;
    }

    // Store device context physical address
    xhci_hcd->device_context_physical[slot_id] = device_ctx_alloc.value;

    // Update DCBAA to point to this device context
    xhci_hcd->dcbaa->device_context_ptrs[slot_id] = device_ctx_alloc.value;

    hcd_debugf("xHCI HCD: Device context set up - slot %u, port %u, speed %u, "
               "max_pkt %u\n",
               slot_id, device->port_number, device->speed, max_packet_size);
    hcd_debugf("xHCI HCD: EP0 ring @ phys 0x%016lx\n", ep0_ring->trbs_physical);
    hcd_debugf(
            "xHCI HCD: Device context @ phys 0x%016lx, DCBAA[%u] = 0x%016lx\n",
            device_ctx_alloc.value, slot_id,
            xhci_hcd->dcbaa->device_context_ptrs[slot_id]);

    return 0;
}

int xhci_submit_control_transfer(XhciHostController *xhci_hcd,
                                 UsbTransfer *transfer) {
    if (!xhci_hcd || !transfer || !transfer->setup_packet) {
        return -1;
    }

    const UsbDeviceRequest *setup = transfer->setup_packet;
    hcd_debugf(
            "xHCI HCD: Control transfer - bmRequestType=0x%02x bRequest=0x%02x "
            "wValue=0x%04x wIndex=0x%04x wLength=%u\n",
            setup->bmRequestType, setup->bRequest, setup->wValue, setup->wIndex,
            setup->wLength);

    // Get device slot ID
    const uint8_t slot_id = (uint8_t)(uintptr_t)transfer->device->hcd_private;
    if (slot_id == 0) {
        hcd_debugf("xHCI HCD: No device slot allocated for control transfer\n");
        return -1;
    }

    // Check if we have space in transfer ring for the transfer TRBs
    uint32_t trbs_needed = 2; // Setup + Status
    if (setup->wLength > 0) {
        trbs_needed++; // + Data stage
    }

    // Get endpoint 0's transfer ring
    XhciRing *ep0_ring = xhci_hcd->endpoint_rings[slot_id][0];
    if (!ep0_ring) {
        hcd_debugf("xHCI HCD: No endpoint ring for slot %u endpoint 0\n",
                   slot_id);
        return -1;
    }

    if (!xhci_ring_has_space(ep0_ring, trbs_needed)) {
        hcd_debugf("xHCI HCD: Not enough space in endpoint transfer ring\n");
        return -1;
    }

    // Build and enqueue TRBs for the control transfer
    const int result =
            xhci_build_control_transfer_trbs(xhci_hcd, transfer, slot_id);
    if (result != 0) {
        hcd_debugf("xHCI HCD: Failed to build control transfer TRBs\n");
        return -1;
    }

    // Ring doorbell to notify hardware
    hcd_debugf(
            "xHCI HCD: Control transfer TRBs built, ringing doorbell for slot "
            "%u EP0\n",
            slot_id);
    xhci_ring_doorbell(xhci_hcd, slot_id, 1); // DCI 1 = control endpoint

    // Wait for transfer completion
    hcd_debugf("xHCI HCD: Waiting for transfer completion\n");
    const int completion_result =
            xhci_wait_for_transfer_completion(xhci_hcd, 5000);
    if (completion_result != 0) {
        hcd_debugf("xHCI HCD: Transfer completion timeout\n");
        transfer->status = USB_TRANSFER_STATUS_ERROR;
        return -1;
    }

    // Copy data back for IN transfers after completion
    if (setup->wLength > 0 && (setup->bmRequestType & 0x80) &&
        transfer->buffer) {
        const uint64_t data_phys = (uint64_t)(uintptr_t)transfer->hcd_private;
        if (data_phys) {
            const volatile void *data_virt =
                    (volatile void *)(0x800000000 + data_phys);
            memcpy(transfer->buffer, (const void *)data_virt, setup->wLength);

#ifdef DEBUG_XHCI_HCD
            hcd_debugf("xHCI HCD: Copied %u bytes back from device\n",
                       setup->wLength);

            // Debug: dump first few bytes of received data
            hcd_debugf("xHCI HCD: Raw data received: ");
            const uint8_t *data_bytes = (uint8_t *)transfer->buffer;
            for (uint32_t i = 0; i < setup->wLength && i < 16; i++) {
                hcd_debugf("%02x ", data_bytes[i]);
            }
            hcd_debugf("\n");
#endif
        }
    }

    // Mark as completed after successful transfer
    transfer->status = USB_TRANSFER_STATUS_COMPLETED;
    transfer->actual_length = setup->wLength;

    hcd_debugf("xHCI HCD: Control transfer submitted\n");
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

// =============================================================================
// Control Transfer Implementation
// =============================================================================

int xhci_build_control_transfer_trbs(XhciHostController *xhci_hcd,
                                     UsbTransfer *transfer,
                                     const uint8_t slot_id) {
    if (!xhci_hcd || !transfer || !transfer->setup_packet) {
        return -1;
    }

    const UsbDeviceRequest *setup = transfer->setup_packet;
    XhciRing *ring =
            xhci_hcd->endpoint_rings[slot_id]
                                    [0]; // Use endpoint 0's transfer ring

    // Allocate physical memory for setup packet
    const SyscallResultA setup_alloc =
            anos_alloc_physical_pages(4096); // 1 page
    if (setup_alloc.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to allocate setup packet memory\n");
        return -1;
    }

    // Map setup packet memory
    const SyscallResult setup_map = anos_map_physical(
            setup_alloc.value, (void *)(0x700000000 + setup_alloc.value), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (setup_map.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to map setup packet memory\n");
        return -1;
    }

    // Copy setup packet to physical memory
    volatile UsbDeviceRequest *setup_phys =
            (volatile UsbDeviceRequest *)(0x700000000 + setup_alloc.value);
    *setup_phys = *setup;

    // Debug: show what we're sending
    hcd_debugf("xHCI HCD: Setup packet: %02x %02x %04x %04x %04x\n",
               setup->bmRequestType, setup->bRequest, setup->wValue,
               setup->wIndex, setup->wLength);

    hcd_vdebugf(
            "xHCI HCD: Setup phys  : %02x %02x %04x %04x %04x at 0x%016lx\n",
            setup_phys->bmRequestType, setup_phys->bRequest, setup_phys->wValue,
            setup_phys->wIndex, setup_phys->wLength, setup_alloc.value);

    // 1. Setup Stage TRB
    XhciTrb *setup_trb = xhci_ring_enqueue_trb(ring);
    if (!setup_trb) {
        hcd_debugf("xHCI HCD: Failed to enqueue setup TRB\n");
        return -1;
    }

    // Try immediate data mode - embed setup packet in TRB parameter field
    uint64_t setup_data = 0;
    memcpy(&setup_data, setup, 8); // Copy 8 bytes of setup packet

    setup_trb->parameter = setup_data; // Embed setup packet directly
    setup_trb->status = 8; // Transfer Length = 8 bytes (immediate mode)

    setup_trb->control = TRB_CONTROL_CYCLE_BIT |
                         (TRB_TYPE_SETUP_STAGE << TRB_CONTROL_TYPE_SHIFT) |
                         TRB_CONTROL_IMMEDIATE_DATA;

    // Set transfer type in setup TRB based on data direction
    if (setup->wLength > 0) {
        if (setup->bmRequestType & 0x80) {
            // Device to host - IN data stage
            setup_trb->control |= (3 << 16); // TRT = IN data stage
        } else {
            // Host to device - OUT data stage
            setup_trb->control |= (2 << 16); // TRT = OUT data stage
        }
    } else {
        // No data stage
        setup_trb->control |= (0 << 16); // TRT = No data stage
    }

    xhci_ring_inc_enqueue(ring);

    // 2. Data Stage TRB (if needed)
    if (setup->wLength > 0 && transfer->buffer) {
        // Allocate and map data buffer
        const size_t data_pages = (setup->wLength + 4095) / 4096;
        const size_t data_size = data_pages * 4096;
        const SyscallResultA data_alloc = anos_alloc_physical_pages(data_size);

        if (data_alloc.result != SYSCALL_OK) {
            hcd_debugf("xHCI HCD: Failed to allocate data buffer\n");
            return -1;
        }

        const SyscallResult data_map = anos_map_physical(
                data_alloc.value, (void *)(0x800000000 + data_alloc.value),
                data_size,
                ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                        ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

        if (data_map.result != SYSCALL_OK) {
            hcd_debugf("xHCI HCD: Failed to map data buffer\n");
            return -1;
        }

        // Copy data if OUT transfer
        if (!(setup->bmRequestType & 0x80) && transfer->buffer) {
            memcpy((void *)(0x800000000 + data_alloc.value), transfer->buffer,
                   setup->wLength);
        }

        XhciTrb *data_trb = xhci_ring_enqueue_trb(ring);
        if (!data_trb) {
            hcd_debugf("xHCI HCD: Failed to enqueue data TRB\n");
            return -1;
        }

        data_trb->parameter = data_alloc.value;
        data_trb->status = setup->wLength;
        data_trb->control = TRB_CONTROL_CYCLE_BIT |
                            (TRB_TYPE_DATA_STAGE << TRB_CONTROL_TYPE_SHIFT);

        // Set direction bit
        if (setup->bmRequestType & 0x80) {
            data_trb->control |= (1 << 16); // DIR = IN
        }

        xhci_ring_inc_enqueue(ring);

        // Store data buffer info for later copying back
        transfer->hcd_private = (void *)(uintptr_t)data_alloc.value;
    }

    // 3. Status Stage TRB
    XhciTrb *status_trb = xhci_ring_enqueue_trb(ring);
    if (!status_trb) {
        hcd_debugf("xHCI HCD: Failed to enqueue status TRB\n");
        return -1;
    }

    status_trb->parameter = 0;
    status_trb->status = 0;
    status_trb->control = TRB_CONTROL_CYCLE_BIT |
                          (TRB_TYPE_STATUS_STAGE << TRB_CONTROL_TYPE_SHIFT) |
                          TRB_CONTROL_INTERRUPT_ON_COMPLETE;

    // Status stage direction is opposite of data stage
    if (setup->wLength > 0) {
        if (setup->bmRequestType & 0x80) {
            // Data was IN, status is OUT (no DIR bit)
        } else {
            // Data was OUT, status is IN
            status_trb->control |= (1 << 16); // DIR = IN
        }
    }

    xhci_ring_inc_enqueue(ring);

    hcd_debugf("xHCI HCD: Built control transfer TRBs\n");
    return 0;
}

void xhci_ring_doorbell(XhciHostController *xhci_hcd, uint8_t slot_id,
                        uint8_t target) {
    if (!xhci_hcd || slot_id == 0) {
        return;
    }

    // Calculate doorbell register offset
    const uint32_t doorbell_offset = slot_id * 4;
    const uint32_t doorbell_value = target; // DCI (Doorbell Control Index)

    hcd_debugf("xHCI HCD: Ringing doorbell for slot %u, target %u "
               "(offset=0x%x, value=0x%x)\n",
               slot_id, target, doorbell_offset, doorbell_value);

    // Write to the actual doorbell register
    if (xhci_hcd->base.doorbell_regs) {
        xhci_write32(&xhci_hcd->base, xhci_hcd->base.doorbell_regs,
                     doorbell_offset, doorbell_value);
        hcd_debugf("xHCI HCD: Doorbell written to hardware\n");
    } else {
        hcd_debugf("xHCI HCD: ERROR - No doorbell registers mapped\n");
    }
}

int xhci_setup_event_ring(XhciHostController *xhci_hcd) {
    if (!xhci_hcd) {
        return -1;
    }

    hcd_debugf("xHCI HCD: Setting up event ring segment table\n");

    // Allocate Event Ring Segment Table (ERST)
    const SyscallResultA erst_alloc = anos_alloc_physical_pages(4096);
    if (erst_alloc.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to allocate ERST\n");
        return -1;
    }

    // Map ERST
    const SyscallResult erst_map = anos_map_physical(
            erst_alloc.value, (void *)(0xA00000000 + erst_alloc.value), 4096,
            ANOS_MAP_PHYSICAL_FLAG_READ | ANOS_MAP_PHYSICAL_FLAG_WRITE |
                    ANOS_MAP_PHYSICAL_FLAG_NOCACHE);

    if (erst_map.result != SYSCALL_OK) {
        hcd_debugf("xHCI HCD: Failed to map ERST\n");
        return -1;
    }

    // Set up ERST entry (16 bytes per entry)
    volatile uint64_t *erst_entry =
            (volatile uint64_t *)(0xA00000000 + erst_alloc.value);
    erst_entry[0] =
            xhci_hcd->event_ring.trbs_physical; // Ring Segment Base Address
    erst_entry[1] = XHCI_RING_SIZE;             // Ring Segment Size

    // Write ERST registers to runtime register space
    if (!xhci_hcd->base.runtime_regs) {
        hcd_debugf("xHCI HCD: ERROR - No runtime registers mapped\n");
        return -1;
    }

    // Interrupter 0 registers (offset 0x20 from runtime base)
    constexpr uint32_t interrupter_base = 0x20;

    // ERSTSZ (Event Ring Segment Table Size) - 1 segment
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x08, 1);

    // CRITICAL: Disable interrupts and use polling mode
    hcd_debugf("xHCI HCD: Disabling interrupts - using polling mode\n");
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x00, 0); // IMAN = 0 (disable interrupts)

    // ERSTBA (Event Ring Segment Table Base Address)
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x10,
                 (uint32_t)(erst_alloc.value & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x14, (uint32_t)(erst_alloc.value >> 32));

    // ERDP (Event Ring Dequeue Pointer)
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x18,
                 (uint32_t)(xhci_hcd->event_ring.trbs_physical & 0xFFFFFFFF));
    xhci_write32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                 interrupter_base + 0x1C,
                 (uint32_t)(xhci_hcd->event_ring.trbs_physical >> 32));

    hcd_debugf("xHCI HCD: Event ring configured - ERST at 0x%016lx, ring at "
               "0x%016lx\n",
               erst_alloc.value, xhci_hcd->event_ring.trbs_physical);

#ifdef DEBUG_XHCI_HCD
#ifdef VERY_NOISY_XHCI_HCD
    // Verify ERST registers were written correctly
    uint32_t erstsz_read =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                        interrupter_base + 0x08);
    uint32_t erstba_low =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                        interrupter_base + 0x10);
    uint32_t erstba_high =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                        interrupter_base + 0x14);
    uint32_t erdp_low =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                        interrupter_base + 0x18);
    uint32_t erdp_high =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.runtime_regs,
                        interrupter_base + 0x1C);

    hcd_debugf("xHCI HCD: ERST registers verification:\n");
    hcd_debugf("  ERSTSZ: 0x%08x (should be 1)\n", erstsz_read);
    hcd_debugf("  ERSTBA: 0x%08x%08x (should be 0x%016lx)\n", erstba_high,
               erstba_low, erst_alloc.value);
    hcd_debugf("  ERDP: 0x%08x%08x (should be 0x%016lx)\n", erdp_high, erdp_low,
               xhci_hcd->event_ring.trbs_physical);

    // Verify CRCR (Command Ring Control Register)
    uint32_t crcr_low =
            xhci_read32(&xhci_hcd->base, xhci_hcd->base.op_regs, XHCI_OP_CRCR);
    uint32_t crcr_high = xhci_read32(&xhci_hcd->base, xhci_hcd->base.op_regs,
                                     XHCI_OP_CRCR + 4);
    uint64_t expected_crcr = xhci_hcd->command_ring.trbs_physical | 0x1;
    printf("  CRCR: 0x%08x%08x (should be 0x%016lx)\n", crcr_high, crcr_low,
           expected_crcr);
#endif
#endif

    return 0;
}