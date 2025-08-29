/*
 * Common filesystem type definitions
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __ANOS_FILESYSTEM_TYPES_H
#define __ANOS_FILESYSTEM_TYPES_H

#include <stdint.h>

// Filesystem message types (start from 100 to avoid conflicts with device messages)
typedef enum {
    FS_MSG_REGISTER = 100,
    FS_MSG_UNREGISTER = 101,
    FS_MSG_MOUNT = 102,
    FS_MSG_UNMOUNT = 103,
    FS_MSG_QUERY_OBJECT_SIZE = 104,
    FS_MSG_LOAD_OBJECT_PAGE = 105,
    FS_MSG_WRITE_OBJECT_PAGE = 106,
    FS_MSG_CREATE_OBJECT = 107,
    FS_MSG_DELETE_OBJECT = 108
} FilesystemMessageType;

// Storage I/O message types for filesystem-to-storage communication
typedef enum {
    STORAGE_MSG_READ_SECTORS = 1,
    STORAGE_MSG_WRITE_SECTORS = 2,
    STORAGE_MSG_GET_INFO = 3,
    STORAGE_MSG_FLUSH = 4
} StorageIOMessageType;

// Filesystem types
typedef enum {
    FS_TYPE_UNKNOWN = 0,
    FS_TYPE_FAT32 = 1,
    FS_TYPE_EXT4 = 2,
    FS_TYPE_RAMFS = 3,
    FS_TYPE_TMPFS = 4
} FilesystemType;

// Filesystem capabilities flags
#define FS_CAP_READ (1 << 0)
#define FS_CAP_WRITE (1 << 1)
#define FS_CAP_EXECUTE (1 << 2)
#define FS_CAP_CREATE (1 << 3)
#define FS_CAP_DELETE (1 << 4)
#define FS_CAP_RESIZE (1 << 5)

// Filesystem information
typedef struct {
    uint64_t fs_id;             // Unique filesystem identifier
    FilesystemType fs_type;     // Type of filesystem
    uint32_t capabilities;      // Filesystem capabilities bitmask
    char mount_prefix[32];      // Mount prefix (e.g., "disk0:", "usb:")
    char fs_name[64];           // Human-readable filesystem name
    char driver_name[32];       // Driver handling this filesystem
    uint64_t driver_channel;    // IPC channel to filesystem driver
    uint64_t storage_device_id; // Storage device this filesystem is on
} FilesystemInfo;

// Filesystem registration message
typedef struct {
    FilesystemMessageType msg_type;
    FilesystemType fs_type;
    uint64_t storage_device_id; // Storage device to mount on
    char mount_prefix[32];      // Desired mount prefix
    char data[];                // Variable-length filesystem info
} FilesystemRegistrationMessage;

// Mount request message
typedef struct {
    FilesystemMessageType msg_type;
    uint64_t storage_device_id;
    char mount_prefix[32];
} FilesystemMountMessage;

// Storage I/O request from filesystem to storage device
typedef struct {
    StorageIOMessageType msg_type;
    uint64_t start_sector; // Starting sector number
    uint32_t sector_count; // Number of sectors to read/write
    uint32_t reserved;     // Padding for alignment
    char data[];           // Data buffer for writes, response buffer for reads
} StorageIOMessage;

// Storage device information response
typedef struct {
    uint64_t sector_count;
    uint32_t sector_size;
    uint32_t capabilities;
    char model[64];
    char serial[32];
} StorageInfoResponse;

// File object size query
typedef struct {
    FilesystemMessageType msg_type;
    char path[]; // Path to file object
} FilesystemObjectSizeQuery;

// File object size response
typedef struct {
    uint64_t object_size; // Size in bytes
    uint32_t error_code;  // 0 = success
    uint32_t reserved;    // Padding for alignment
} FilesystemObjectSizeResponse;

// File object page load request
typedef struct {
    FilesystemMessageType msg_type;
    uint64_t page_offset; // Offset in file (page-aligned)
    uint32_t page_count;  // Number of pages to load
    uint32_t reserved;    // Padding for alignment
    char path[];          // Path to file object
} FilesystemObjectPageLoad;

// File object page load response
typedef struct {
    uint32_t pages_loaded; // Number of pages actually loaded
    uint32_t error_code;   // 0 = success
    char data[];           // Page data
} FilesystemObjectPageResponse;

// VFS registration with SYSTEM
typedef struct {
    char mount_prefix[32];      // Mount prefix this driver handles
    uint64_t fs_driver_channel; // Channel to send filesystem requests to
} VFSMountEntry;

// VFS message types for SYSTEM::VFS
#define VFS_FIND_FS_DRIVER 1
#define VFS_REGISTER_FILESYSTEM 2
#define VFS_UNREGISTER_FILESYSTEM 3

#endif