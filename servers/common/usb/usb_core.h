/*
 * USB Core Layer Interface
 * anos - An Operating System
 *
 * Hardware-agnostic USB protocol implementation
 * Provides abstraction between USB devices and host controllers
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_USB_CORE_H
#define __ANOS_USB_CORE_H

#include "usb_spec.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct UsbDevice UsbDevice;
typedef struct UsbHostController UsbHostController;
typedef struct UsbTransfer UsbTransfer;

// =============================================================================
// USB Transfer Types and Structures
// =============================================================================

typedef enum {
    USB_TRANSFER_CONTROL,
    USB_TRANSFER_BULK,
    USB_TRANSFER_INTERRUPT,
    USB_TRANSFER_ISOCHRONOUS
} UsbTransferType;

typedef enum {
    USB_TRANSFER_STATUS_PENDING,
    USB_TRANSFER_STATUS_COMPLETED,
    USB_TRANSFER_STATUS_ERROR,
    USB_TRANSFER_STATUS_CANCELLED,
    USB_TRANSFER_STATUS_TIMEOUT
} UsbTransferStatus;

// USB Transfer completion callback
typedef void (*UsbTransferCallback)(UsbTransfer *transfer, void *user_data);

// USB Transfer structure
struct UsbTransfer {
    UsbDevice *device;    // Target device
    uint8_t endpoint;     // Endpoint address
    UsbTransferType type; // Transfer type

    // For control transfers
    UsbDeviceRequest *setup_packet; // Setup packet (control only)

    // Data buffer
    void *buffer;         // Data buffer
    size_t buffer_length; // Buffer size
    size_t actual_length; // Actual transferred bytes

    // Completion handling
    UsbTransferCallback callback; // Completion callback
    void *user_data;              // User data for callback
    UsbTransferStatus status;     // Transfer status

    // Timeout and retry
    uint32_t timeout_ms; // Timeout in milliseconds

    // Internal use by host controller
    void *hcd_private; // Host controller private data
};

// =============================================================================
// USB Device Structure
// =============================================================================

typedef enum {
    USB_DEVICE_STATE_DEFAULT,    // Default state after reset
    USB_DEVICE_STATE_ADDRESS,    // Address assigned
    USB_DEVICE_STATE_CONFIGURED, // Configuration set
    USB_DEVICE_STATE_SUSPENDED,  // Suspended
    USB_DEVICE_STATE_ERROR       // Error state
} UsbDeviceState;

struct UsbDevice {
    // Device identification
    uint8_t address;      // USB device address (1-127)
    uint8_t port_number;  // Port on hub/controller
    uint8_t speed;        // USB_SPEED_* constant
    UsbDeviceState state; // Current device state

    // Device descriptors
    UsbDeviceDescriptor device_desc;         // Device descriptor
    UsbConfigurationDescriptor *config_desc; // Active configuration
    uint8_t *raw_config_desc;      // Raw configuration descriptor data
    size_t raw_config_desc_length; // Length of raw config data

    // String descriptors (cached)
    char manufacturer[64];  // Manufacturer string
    char product[64];       // Product string
    char serial_number[32]; // Serial number string

    // Host controller association
    UsbHostController *hcd; // Host controller driver
    void *hcd_private;      // Host controller private data

    // Parent/child relationships
    UsbDevice *parent;       // Parent device (hub)
    UsbDevice *children[16]; // Child devices (for hubs)
    uint8_t num_children;    // Number of child devices

    // Device driver association
    void *driver;         // Device driver instance
    void *driver_private; // Driver private data
};

// =============================================================================
// Host Controller Driver Interface
// =============================================================================

// Host Controller Driver operations
typedef struct {
    // Transfer operations
    int (*submit_transfer)(UsbHostController *hcd, UsbTransfer *transfer);
    int (*cancel_transfer)(UsbHostController *hcd, UsbTransfer *transfer);

    // Device management
    int (*reset_device)(UsbHostController *hcd, UsbDevice *device);
    int (*set_address)(UsbHostController *hcd, UsbDevice *device,
                       uint8_t address);
    int (*enable_device)(UsbHostController *hcd, UsbDevice *device);
    int (*disable_device)(UsbHostController *hcd, UsbDevice *device);

    // Endpoint management
    int (*configure_endpoint)(UsbHostController *hcd, UsbDevice *device,
                              UsbEndpointDescriptor *ep_desc);

    // Port management
    int (*get_port_status)(UsbHostController *hcd, uint8_t port,
                           uint32_t *status);
    int (*reset_port)(UsbHostController *hcd, uint8_t port);
    int (*enable_port)(UsbHostController *hcd, uint8_t port);
    int (*disable_port)(UsbHostController *hcd, uint8_t port);
} UsbHostControllerOps;

// Host Controller Driver structure
struct UsbHostController {
    const char *name;          // Controller name
    UsbHostControllerOps *ops; // Controller operations

    // Controller capabilities
    uint8_t max_devices;       // Maximum devices supported
    uint8_t num_ports;         // Number of root hub ports
    uint32_t supported_speeds; // Bitmask of supported speeds

    // Device management
    UsbDevice *devices[128]; // Device array (address 1-127)
    UsbDevice *root_hub;     // Virtual root hub device

    // Private data for controller implementation
    void *private_data; // Controller-specific private data
};

// =============================================================================
// USB Core API Functions
// =============================================================================

// Host Controller Registration
int usb_register_host_controller(UsbHostController *hcd);
int usb_unregister_host_controller(UsbHostController *hcd);

// Device Enumeration and Management
UsbDevice *usb_alloc_device(UsbHostController *hcd, uint8_t port_number,
                            uint8_t speed);
void usb_free_device(UsbDevice *device);
int usb_enumerate_device(UsbDevice *device);
int usb_configure_device(UsbDevice *device, uint8_t config_value);

// Transfer Management
UsbTransfer *usb_alloc_transfer(UsbDevice *device, uint8_t endpoint,
                                UsbTransferType type);
void usb_free_transfer(UsbTransfer *transfer);
int usb_submit_transfer(UsbTransfer *transfer);
int usb_cancel_transfer(UsbTransfer *transfer);

// Control Transfer Helpers
int usb_control_transfer(UsbDevice *device, uint8_t request_type,
                         uint8_t request, uint16_t value, uint16_t index,
                         void *data, uint16_t length, uint32_t timeout_ms);

int usb_get_descriptor(UsbDevice *device, uint8_t desc_type, uint8_t desc_index,
                       void *buffer, size_t buffer_length);

int usb_set_address(UsbDevice *device, uint8_t address);
int usb_set_configuration(UsbDevice *device, uint8_t config_value);

// String Descriptor Helpers
int usb_get_string_descriptor(UsbDevice *device, uint8_t string_index,
                              char *buffer, size_t buffer_size);

// Device Information
const char *usb_get_speed_string(uint8_t speed);
const char *usb_get_class_string(uint8_t device_class);

// Initialization
int usb_core_init(void);
void usb_core_shutdown(void);

#endif