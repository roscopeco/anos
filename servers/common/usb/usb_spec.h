/*
 * USB Specification Definitions
 * anos - An Operating System
 *
 * Standard USB 3.0/2.0 protocol definitions
 * Hardware-agnostic USB specification constants
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_USB_SPEC_H
#define __ANOS_USB_SPEC_H

#include <stdint.h>

// =============================================================================
// USB Standard Request Types
// =============================================================================

// bmRequestType field breakdown
#define USB_REQ_TYPE_DIR_MASK 0x80
#define USB_REQ_TYPE_DIR_HOST_TO_DEV 0x00
#define USB_REQ_TYPE_DIR_DEV_TO_HOST 0x80

#define USB_REQ_TYPE_TYPE_MASK 0x60
#define USB_REQ_TYPE_STANDARD 0x00
#define USB_REQ_TYPE_CLASS 0x20
#define USB_REQ_TYPE_VENDOR 0x40

#define USB_REQ_TYPE_RECIP_MASK 0x1F
#define USB_REQ_TYPE_DEVICE 0x00
#define USB_REQ_TYPE_INTERFACE 0x01
#define USB_REQ_TYPE_ENDPOINT 0x02
#define USB_REQ_TYPE_OTHER 0x03

// Standard Request Codes (bRequest)
#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B
#define USB_REQ_SYNCH_FRAME 0x0C

// =============================================================================
// USB Descriptor Types
// =============================================================================

#define USB_DESC_TYPE_DEVICE 0x01
#define USB_DESC_TYPE_CONFIGURATION 0x02
#define USB_DESC_TYPE_STRING 0x03
#define USB_DESC_TYPE_INTERFACE 0x04
#define USB_DESC_TYPE_ENDPOINT 0x05
#define USB_DESC_TYPE_DEVICE_QUALIFIER 0x06
#define USB_DESC_TYPE_OTHER_SPEED 0x07
#define USB_DESC_TYPE_INTERFACE_POWER 0x08
#define USB_DESC_TYPE_BOS 0x0F

// =============================================================================
// USB Descriptor Structures
// =============================================================================

// USB Device Request (Setup Packet)
typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) UsbDeviceRequest;

// USB Device Descriptor
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) UsbDeviceDescriptor;

// USB Configuration Descriptor
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed)) UsbConfigurationDescriptor;

// USB Interface Descriptor
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) UsbInterfaceDescriptor;

// USB Endpoint Descriptor
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) UsbEndpointDescriptor;

// =============================================================================
// USB Device Classes
// =============================================================================

#define USB_CLASS_UNDEFINED 0x00
#define USB_CLASS_AUDIO 0x01
#define USB_CLASS_CDC 0x02
#define USB_CLASS_HID 0x03
#define USB_CLASS_PHYSICAL 0x05
#define USB_CLASS_IMAGE 0x06
#define USB_CLASS_PRINTER 0x07
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_CLASS_HUB 0x09
#define USB_CLASS_CDC_DATA 0x0A
#define USB_CLASS_SMART_CARD 0x0B
#define USB_CLASS_CONTENT_SECURITY 0x0D
#define USB_CLASS_VIDEO 0x0E
#define USB_CLASS_PERSONAL_HEALTHCARE 0x0F
#define USB_CLASS_DIAGNOSTIC_DEVICE 0xDC
#define USB_CLASS_WIRELESS 0xE0
#define USB_CLASS_MISCELLANEOUS 0xEF
#define USB_CLASS_APP_SPECIFIC 0xFE
#define USB_CLASS_VENDOR_SPECIFIC 0xFF

// =============================================================================
// USB Device Speeds
// =============================================================================

#define USB_SPEED_UNKNOWN 0
#define USB_SPEED_LOW 1        // 1.5 Mbps
#define USB_SPEED_FULL 2       // 12 Mbps
#define USB_SPEED_HIGH 3       // 480 Mbps
#define USB_SPEED_SUPER 4      // 5 Gbps
#define USB_SPEED_SUPER_PLUS 5 // 10 Gbps

// =============================================================================
// USB Endpoint Types and Directions
// =============================================================================

#define USB_ENDPOINT_DIR_MASK 0x80
#define USB_ENDPOINT_DIR_OUT 0x00
#define USB_ENDPOINT_DIR_IN 0x80

#define USB_ENDPOINT_TYPE_MASK 0x03
#define USB_ENDPOINT_TYPE_CONTROL 0x00
#define USB_ENDPOINT_TYPE_ISOC 0x01
#define USB_ENDPOINT_TYPE_BULK 0x02
#define USB_ENDPOINT_TYPE_INT 0x03

// =============================================================================
// USB Configuration Attributes
// =============================================================================

#define USB_CONFIG_ATT_SELF_POWERED 0x40
#define USB_CONFIG_ATT_REMOTE_WAKEUP 0x20

// =============================================================================
// USB String Descriptor Language IDs
// =============================================================================

#define USB_LANGID_ENGLISH_US 0x0409

#endif