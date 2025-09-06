/*
 * Common device type definitions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_DEVICE_TYPES_H
#define __ANOS_DEVICE_TYPES_H

#include <stdint.h>

// Device message types
typedef enum {
    DEVICE_MSG_REGISTER = 1,
    DEVICE_MSG_UNREGISTER = 2,
    DEVICE_MSG_QUERY = 3,
    DEVICE_MSG_ENUMERATE = 4
} DeviceMessageType;

// Device types
typedef enum {
    DEVICE_TYPE_UNKNOWN = 0,
    DEVICE_TYPE_PCI = 1,
    DEVICE_TYPE_STORAGE = 2,
    DEVICE_TYPE_NETWORK = 3,
    DEVICE_TYPE_DISPLAY = 4,
    DEVICE_TYPE_USB = 5
} DeviceType;

// Hardware types for storage devices
typedef enum {
    STORAGE_HW_UNKNOWN = 0,
    STORAGE_HW_AHCI = 1,
    STORAGE_HW_NVME = 2,
    STORAGE_HW_IDE = 3,
    STORAGE_HW_USB = 4
} StorageHardwareType;

// Hardware types for USB devices
typedef enum {
    USB_HW_UNKNOWN = 0,
    USB_HW_XHCI = 1,
    USB_HW_EHCI = 2,
    USB_HW_UHCI = 3,
    USB_HW_OHCI = 4
} UsbHardwareType;

// Query types
typedef enum {
    QUERY_BY_TYPE = 1,
    QUERY_BY_ID = 2,
    QUERY_ALL = 3,
    QUERY_CHILDREN = 4
} DeviceQueryType;

// Device capabilities flags
#define DEVICE_CAP_READ (1 << 0)
#define DEVICE_CAP_WRITE (1 << 1)
#define DEVICE_CAP_HOTPLUG (1 << 2)
#define DEVICE_CAP_REMOVABLE (1 << 3)
#define DEVICE_CAP_TRIM (1 << 4)

// Common device information
typedef struct {
    uint64_t device_id;      // Unique device identifier
    uint64_t parent_id;      // Parent device ID (0 for root)
    DeviceType device_type;  // Type of device
    uint32_t hardware_type;  // Hardware-specific type
    uint32_t capabilities;   // Device capabilities bitmask
    char name[64];           // Human-readable device name
    char driver_name[32];    // Driver handling this device
    uint64_t driver_channel; // IPC channel to driver
} DeviceInfo;

// Storage-specific device information
typedef struct {
    DeviceInfo base;
    uint64_t sector_count; // Total sectors on device
    uint32_t sector_size;  // Size of each sector in bytes
    char model[64];        // Device model string
    char serial[32];       // Device serial number
} StorageDeviceInfo;

// PCI-specific device information
typedef struct {
    DeviceInfo base;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint64_t config_base; // PCI config space base address
} PciDeviceInfo;

// USB-specific device information
typedef struct {
    DeviceInfo base;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t port_number;
    uint8_t device_speed;
    char manufacturer[64];
    char product[64];
    char serial_number[32];
} UsbDeviceInfo;

// Device registration message
typedef struct {
    DeviceMessageType msg_type;
    DeviceType device_type;
    uint32_t device_count; // Number of devices being registered
    char data[];           // Variable-length device info array
} DeviceRegistrationMessage;

// Device query message
typedef struct {
    DeviceMessageType msg_type;
    DeviceQueryType query_type;
    DeviceType device_type; // For QUERY_BY_TYPE
    uint64_t device_id;     // For QUERY_BY_ID or QUERY_CHILDREN
} DeviceQueryMessage;

// Device query response
typedef struct {
    uint32_t device_count;
    uint32_t error_code; // 0 = success
    char data[];         // Variable-length device info array
} DeviceQueryResponse;

#endif