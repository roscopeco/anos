/*
 * USB Core Layer Implementation
 * anos - An Operating System
 *
 * Hardware-agnostic USB protocol implementation
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "usb_core.h"
#include "usb_spec.h"

// =============================================================================
// Global USB Core State
// =============================================================================

static UsbHostController *registered_hcds[4] = {
        NULL}; // Support up to 4 controllers
static uint8_t num_registered_hcds = 0;
static bool usb_core_initialized = false;

// =============================================================================
// Host Controller Management
// =============================================================================

int usb_register_host_controller(UsbHostController *hcd) {
    if (!hcd || !hcd->ops) {
        return -1;
    }

    if (num_registered_hcds >= 4) {
        printf("USB: Maximum number of host controllers reached\n");
        return -1;
    }

    registered_hcds[num_registered_hcds++] = hcd;
    printf("USB: Registered host controller '%s' with %u ports\n", hcd->name,
           hcd->num_ports);

    return 0;
}

int usb_unregister_host_controller(UsbHostController *hcd) {
    for (uint8_t i = 0; i < num_registered_hcds; i++) {
        if (registered_hcds[i] == hcd) {
            // Shift remaining controllers down
            for (uint8_t j = i; j < num_registered_hcds - 1; j++) {
                registered_hcds[j] = registered_hcds[j + 1];
            }
            registered_hcds[--num_registered_hcds] = NULL;
            printf("USB: Unregistered host controller '%s'\n", hcd->name);
            return 0;
        }
    }
    return -1;
}

// =============================================================================
// Device Management
// =============================================================================

UsbDevice *usb_alloc_device(UsbHostController *hcd, uint8_t port_number,
                            uint8_t speed) {
    if (!hcd)
        return NULL;

    UsbDevice *device = calloc(1, sizeof(UsbDevice));
    if (!device) {
        printf("USB: Failed to allocate device structure\n");
        return NULL;
    }

    device->hcd = hcd;
    device->port_number = port_number;
    device->speed = speed;
    device->state = USB_DEVICE_STATE_DEFAULT;
    device->address = 0; // Will be assigned during enumeration

    printf("USB: Allocated device on port %u (speed: %s)\n", port_number,
           usb_get_speed_string(speed));

    return device;
}

void usb_free_device(UsbDevice *device) {
    if (!device)
        return;

    // Free configuration descriptor if allocated
    if (device->raw_config_desc) {
        free(device->raw_config_desc);
    }

    // Remove from host controller's device list
    if (device->hcd && device->address > 0) {
        device->hcd->devices[device->address] = NULL;
    }

    free(device);
}

// =============================================================================
// Device Enumeration
// =============================================================================

int usb_enumerate_device(UsbDevice *device) {
    if (!device || !device->hcd || !device->hcd->ops) {
        return -1;
    }

    printf("USB: Enumerating device on port %u\n", device->port_number);

    // Step 1: Get device descriptor (first 8 bytes for max packet size)
    UsbDeviceDescriptor partial_desc;
    int result = usb_get_descriptor(device, USB_DESC_TYPE_DEVICE, 0,
                                    &partial_desc, 8);
    if (result < 0) {
        printf("USB: Failed to get partial device descriptor\n");
        return -1;
    }

    // Step 2: Assign device address
    uint8_t new_address = 1; // TODO: Find free address
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (device->hcd->devices[addr] == NULL) {
            new_address = addr;
            break;
        }
    }

    result = usb_set_address(device, new_address);
    if (result < 0) {
        printf("USB: Failed to set device address %u\n", new_address);
        return -1;
    }

    device->address = new_address;
    device->state = USB_DEVICE_STATE_ADDRESS;
    device->hcd->devices[new_address] = device;

    // Step 3: Get full device descriptor
    result = usb_get_descriptor(device, USB_DESC_TYPE_DEVICE, 0,
                                &device->device_desc,
                                sizeof(UsbDeviceDescriptor));
    if (result < 0) {
        printf("USB: Failed to get full device descriptor\n");
        return -1;
    }

    printf("USB: Device enumerated - VID:0x%04x PID:0x%04x Class:0x%02x\n",
           device->device_desc.idVendor, device->device_desc.idProduct,
           device->device_desc.bDeviceClass);

    // Step 4: Get string descriptors
    if (device->device_desc.iManufacturer) {
        usb_get_string_descriptor(device, device->device_desc.iManufacturer,
                                  device->manufacturer,
                                  sizeof(device->manufacturer));
    }

    if (device->device_desc.iProduct) {
        usb_get_string_descriptor(device, device->device_desc.iProduct,
                                  device->product, sizeof(device->product));
    }

    if (device->device_desc.iSerialNumber) {
        usb_get_string_descriptor(device, device->device_desc.iSerialNumber,
                                  device->serial_number,
                                  sizeof(device->serial_number));
    }

    return 0;
}

int usb_configure_device(UsbDevice *device, uint8_t config_value) {
    if (!device || device->state != USB_DEVICE_STATE_ADDRESS) {
        return -1;
    }

    // Get configuration descriptor
    UsbConfigurationDescriptor config_desc;
    int result = usb_get_descriptor(device, USB_DESC_TYPE_CONFIGURATION,
                                    config_value - 1, &config_desc,
                                    sizeof(UsbConfigurationDescriptor));
    if (result < 0) {
        printf("USB: Failed to get configuration descriptor\n");
        return -1;
    }

    // Get full configuration descriptor with all interfaces and endpoints
    device->raw_config_desc = malloc(config_desc.wTotalLength);
    if (!device->raw_config_desc) {
        printf("USB: Failed to allocate configuration descriptor buffer\n");
        return -1;
    }

    result = usb_get_descriptor(device, USB_DESC_TYPE_CONFIGURATION,
                                config_value - 1, device->raw_config_desc,
                                config_desc.wTotalLength);
    if (result < 0) {
        printf("USB: Failed to get full configuration descriptor\n");
        free(device->raw_config_desc);
        device->raw_config_desc = NULL;
        return -1;
    }

    device->raw_config_desc_length = config_desc.wTotalLength;
    device->config_desc = (UsbConfigurationDescriptor *)device->raw_config_desc;

    // Set configuration
    result = usb_control_transfer(
            device,
            USB_REQ_TYPE_DIR_HOST_TO_DEV | USB_REQ_TYPE_STANDARD |
                    USB_REQ_TYPE_DEVICE,
            USB_REQ_SET_CONFIGURATION, config_value, 0, NULL, 0, 5000);
    if (result < 0) {
        printf("USB: Failed to set configuration %u\n", config_value);
        return -1;
    }

    device->state = USB_DEVICE_STATE_CONFIGURED;
    printf("USB: Device configured with configuration %u\n", config_value);

    return 0;
}

// =============================================================================
// Transfer Management
// =============================================================================

UsbTransfer *usb_alloc_transfer(UsbDevice *device, uint8_t endpoint,
                                UsbTransferType type) {
    if (!device)
        return NULL;

    UsbTransfer *transfer = calloc(1, sizeof(UsbTransfer));
    if (!transfer) {
        printf("USB: Failed to allocate transfer structure\n");
        return NULL;
    }

    transfer->device = device;
    transfer->endpoint = endpoint;
    transfer->type = type;
    transfer->status = USB_TRANSFER_STATUS_PENDING;
    transfer->timeout_ms = 5000; // Default 5 second timeout

    return transfer;
}

void usb_free_transfer(UsbTransfer *transfer) {
    if (transfer) {
        free(transfer);
    }
}

int usb_submit_transfer(UsbTransfer *transfer) {
    if (!transfer || !transfer->device || !transfer->device->hcd) {
        return -1;
    }

    UsbHostController *hcd = transfer->device->hcd;
    if (!hcd->ops || !hcd->ops->submit_transfer) {
        return -1;
    }

    return hcd->ops->submit_transfer(hcd, transfer);
}

int usb_cancel_transfer(UsbTransfer *transfer) {
    if (!transfer || !transfer->device || !transfer->device->hcd) {
        return -1;
    }

    UsbHostController *hcd = transfer->device->hcd;
    if (!hcd->ops || !hcd->ops->cancel_transfer) {
        return -1;
    }

    return hcd->ops->cancel_transfer(hcd, transfer);
}

// =============================================================================
// Control Transfer Helpers
// =============================================================================

int usb_control_transfer(UsbDevice *device, uint8_t request_type,
                         uint8_t request, uint16_t value, uint16_t index,
                         void *data, uint16_t length, uint32_t timeout_ms) {
    if (!device)
        return -1;

    UsbDeviceRequest setup = {.bmRequestType = request_type,
                              .bRequest = request,
                              .wValue = value,
                              .wIndex = index,
                              .wLength = length};

    UsbTransfer *transfer = usb_alloc_transfer(device, 0, USB_TRANSFER_CONTROL);
    if (!transfer)
        return -1;

    transfer->setup_packet = &setup;
    transfer->buffer = data;
    transfer->buffer_length = length;
    transfer->timeout_ms = timeout_ms;

    int result = usb_submit_transfer(transfer);
    if (result < 0) {
        usb_free_transfer(transfer);
        return result;
    }

    // TODO: Wait for completion synchronously
    // For now, assume immediate completion for control transfers

    int ret_val = (transfer->status == USB_TRANSFER_STATUS_COMPLETED)
                          ? (int)transfer->actual_length
                          : -1;

    usb_free_transfer(transfer);
    return ret_val;
}

int usb_get_descriptor(UsbDevice *device, uint8_t desc_type, uint8_t desc_index,
                       void *buffer, size_t buffer_length) {
    return usb_control_transfer(
            device,
            USB_REQ_TYPE_DIR_DEV_TO_HOST | USB_REQ_TYPE_STANDARD |
                    USB_REQ_TYPE_DEVICE,
            USB_REQ_GET_DESCRIPTOR, (desc_type << 8) | desc_index, 0, buffer,
            buffer_length, 5000);
}

int usb_set_address(UsbDevice *device, uint8_t address) {
    return usb_control_transfer(device,
                                USB_REQ_TYPE_DIR_HOST_TO_DEV |
                                        USB_REQ_TYPE_STANDARD |
                                        USB_REQ_TYPE_DEVICE,
                                USB_REQ_SET_ADDRESS, address, 0, NULL, 0, 5000);
}

int usb_set_configuration(UsbDevice *device, uint8_t config_value) {
    return usb_control_transfer(
            device,
            USB_REQ_TYPE_DIR_HOST_TO_DEV | USB_REQ_TYPE_STANDARD |
                    USB_REQ_TYPE_DEVICE,
            USB_REQ_SET_CONFIGURATION, config_value, 0, NULL, 0, 5000);
}

// =============================================================================
// String Descriptor Helpers
// =============================================================================

int usb_get_string_descriptor(UsbDevice *device, uint8_t string_index,
                              char *buffer, size_t buffer_size) {
    if (!device || !buffer || buffer_size == 0)
        return -1;

    // Get string descriptor in UTF-16
    uint8_t string_desc[256];
    int result = usb_get_descriptor(device, USB_DESC_TYPE_STRING, string_index,
                                    string_desc, sizeof(string_desc));
    if (result < 0)
        return -1;

    // Convert UTF-16 to ASCII (simplified)
    uint8_t length = string_desc[0];
    if (length < 2)
        return -1;

    size_t str_len = (length - 2) / 2; // UTF-16 string length
    size_t copy_len = (str_len < buffer_size - 1) ? str_len : buffer_size - 1;

    for (size_t i = 0; i < copy_len; i++) {
        buffer[i] = string_desc[2 + i * 2]; // Take low byte of UTF-16 char
    }
    buffer[copy_len] = '\0';

    return (int)copy_len;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char *usb_get_speed_string(uint8_t speed) {
    switch (speed) {
    case USB_SPEED_LOW:
        return "Low Speed (1.5 Mbps)";
    case USB_SPEED_FULL:
        return "Full Speed (12 Mbps)";
    case USB_SPEED_HIGH:
        return "High Speed (480 Mbps)";
    case USB_SPEED_SUPER:
        return "SuperSpeed (5 Gbps)";
    case USB_SPEED_SUPER_PLUS:
        return "SuperSpeed+ (10 Gbps)";
    default:
        return "Unknown Speed";
    }
}

const char *usb_get_class_string(uint8_t device_class) {
    switch (device_class) {
    case USB_CLASS_UNDEFINED:
        return "Undefined";
    case USB_CLASS_AUDIO:
        return "Audio";
    case USB_CLASS_CDC:
        return "CDC";
    case USB_CLASS_HID:
        return "HID";
    case USB_CLASS_PHYSICAL:
        return "Physical";
    case USB_CLASS_IMAGE:
        return "Image";
    case USB_CLASS_PRINTER:
        return "Printer";
    case USB_CLASS_MASS_STORAGE:
        return "Mass Storage";
    case USB_CLASS_HUB:
        return "Hub";
    case USB_CLASS_CDC_DATA:
        return "CDC Data";
    case USB_CLASS_SMART_CARD:
        return "Smart Card";
    case USB_CLASS_CONTENT_SECURITY:
        return "Content Security";
    case USB_CLASS_VIDEO:
        return "Video";
    case USB_CLASS_PERSONAL_HEALTHCARE:
        return "Personal Healthcare";
    case USB_CLASS_DIAGNOSTIC_DEVICE:
        return "Diagnostic Device";
    case USB_CLASS_WIRELESS:
        return "Wireless";
    case USB_CLASS_MISCELLANEOUS:
        return "Miscellaneous";
    case USB_CLASS_APP_SPECIFIC:
        return "Application Specific";
    case USB_CLASS_VENDOR_SPECIFIC:
        return "Vendor Specific";
    default:
        return "Unknown Class";
    }
}

// =============================================================================
// Initialization
// =============================================================================

int usb_core_init(void) {
    if (usb_core_initialized) {
        return 0;
    }

    memset(registered_hcds, 0, sizeof(registered_hcds));
    num_registered_hcds = 0;

    usb_core_initialized = true;
    printf("USB: Core layer initialized\n");

    return 0;
}

void usb_core_shutdown(void) {
    if (!usb_core_initialized) {
        return;
    }

    // TODO: Clean up registered controllers and devices

    usb_core_initialized = false;
    printf("USB: Core layer shutdown\n");
}