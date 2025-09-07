/*
 * xHCI Host Controller Driver Interface
 * anos - An Operating System
 *
 * xHCI implementation of USB Core Host Controller Interface
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_XHCI_HCD_H
#define __ANOS_XHCI_HCD_H

#include <stdbool.h>
#include <stdint.h>

#include "../../common/usb/usb_core.h"
#include "xhci.h"
#include "xhci_rings.h"

// =============================================================================
// xHCI Device Context Structures
// =============================================================================

// Device Context Base Address Array (DCBAA)
#define XHCI_MAX_DEVICES 128

typedef struct {
    uint64_t device_context_ptrs[XHCI_MAX_DEVICES];
} __attribute__((packed)) XhciDcbaa;

// Slot Context
typedef struct {
    uint32_t info1;   // Route string, speed, MTT, hub, context entries
    uint32_t info2;   // Max exit latency, root hub port number, num ports
    uint32_t tt_info; // TT hub slot ID, TT port number, TT think time
    uint32_t state;   // Slot state, device address
    uint32_t reserved[4];
} __attribute__((packed)) XhciSlotContext;

// Endpoint Context
typedef struct {
    uint32_t info1; // Endpoint state, mult, max primary streams, LSA, interval
    uint32_t
            info2; // Error count, endpoint type, HID, max burst size, max packet size
    uint64_t dequeue_ptr; // TR dequeue pointer and DCS bit
    uint32_t tx_info;     // Average TRB length, max ESIT payload
    uint32_t reserved[3];
} __attribute__((packed)) XhciEndpointContext;

// Device Context (1 slot context + 31 endpoint contexts)
typedef struct {
    XhciSlotContext slot;
    XhciEndpointContext endpoints[31];
} __attribute__((packed)) XhciDeviceContext;

// Input Context (control context + device context)
typedef struct {
    uint32_t drop_flags;  // Drop context flags
    uint32_t add_flags;   // Add context flags
    uint32_t reserved[6]; // Reserved
    XhciDeviceContext device_ctx;
} __attribute__((packed)) XhciInputContext;

// =============================================================================
// xHCI Extended Controller Structure
// =============================================================================

typedef struct {
    // Base xHCI controller
    XHCIController base;

    // USB Core integration
    UsbHostController *usb_hcd; // USB host controller instance

    // Command and event handling
    XhciRing command_ring; // Command ring
    XhciRing event_ring;   // Primary event ring

    // Device context management
    XhciDcbaa *dcbaa;        // Device Context Base Address Array
    uint64_t dcbaa_physical; // Physical address of DCBAA

    XhciInputContext
            *input_contexts[XHCI_MAX_DEVICES]; // Input contexts per slot
    uint64_t input_context_physical[XHCI_MAX_DEVICES]; // Physical addresses

    XhciDeviceContext
            *device_contexts[XHCI_MAX_DEVICES]; // Device contexts per slot
    uint64_t device_context_physical[XHCI_MAX_DEVICES]; // Physical addresses

    // Transfer rings per endpoint per device
    XhciRing *endpoint_rings[XHCI_MAX_DEVICES][32]; // [slot][endpoint]

    // Slot management
    bool slots_enabled[XHCI_MAX_DEVICES]; // Track enabled slots
    uint8_t next_slot_id;                 // Next available slot ID

    // Command completion tracking
    uint32_t command_sequence;       // Command sequence counter
    bool command_completion_pending; // Command completion pending flag

} XhciHostController;

// =============================================================================
// Function Prototypes
// =============================================================================

// Host Controller Setup
int xhci_hcd_init(XhciHostController *xhci_hcd, uint64_t base_addr,
                  uint64_t pci_config_base);
void xhci_hcd_shutdown(XhciHostController *xhci_hcd);

// USB Host Controller Operations (implements UsbHostControllerOps)
int xhci_hcd_submit_transfer(UsbHostController *hcd, UsbTransfer *transfer);
int xhci_hcd_cancel_transfer(UsbHostController *hcd, UsbTransfer *transfer);
int xhci_hcd_reset_device(UsbHostController *hcd, UsbDevice *device);
int xhci_hcd_set_address(UsbHostController *hcd, UsbDevice *device,
                         uint8_t address);
int xhci_hcd_enable_device(UsbHostController *hcd, UsbDevice *device);
int xhci_hcd_disable_device(UsbHostController *hcd, UsbDevice *device);
int xhci_hcd_configure_endpoint(UsbHostController *hcd, UsbDevice *device,
                                UsbEndpointDescriptor *ep_desc);
int xhci_hcd_get_port_status(UsbHostController *hcd, uint8_t port,
                             uint32_t *status);
int xhci_hcd_reset_port(UsbHostController *hcd, uint8_t port);
int xhci_hcd_enable_port(UsbHostController *hcd, uint8_t port);
int xhci_hcd_disable_port(UsbHostController *hcd, uint8_t port);

// Command Processing
int xhci_send_command(XhciHostController *xhci_hcd, XhciTrb *command_trb);
int xhci_wait_for_command_completion(XhciHostController *xhci_hcd,
                                     uint32_t timeout_ms);

// Context Management
int xhci_alloc_device_slot(XhciHostController *xhci_hcd);
void xhci_free_device_slot(XhciHostController *xhci_hcd, uint8_t slot_id);
int xhci_setup_device_context(XhciHostController *xhci_hcd, uint8_t slot_id,
                              UsbDevice *device);

// Event Processing
void xhci_process_events(XhciHostController *xhci_hcd);
void xhci_handle_transfer_event(XhciHostController *xhci_hcd,
                                XhciTransferEventTrb *event);
void xhci_handle_command_completion(XhciHostController *xhci_hcd,
                                    XhciCommandCompletionEventTrb *event);
void xhci_handle_port_status_change(XhciHostController *xhci_hcd,
                                    XhciPortStatusChangeEventTrb *event);

// Utility functions
XhciHostController *xhci_hcd_from_usb_hcd(UsbHostController *hcd);
const char *xhci_slot_state_string(uint32_t state);
const char *xhci_endpoint_state_string(uint32_t state);

#endif