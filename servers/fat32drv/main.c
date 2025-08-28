/*
 * FAT32 Filesystem Driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <anos/syscalls.h>

#include "../common/device_types.h"
#include "../common/filesystem_types.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

static uint64_t fat32_channel = 0;
static uint64_t storage_device_id = 0;
static uint64_t storage_driver_channel = 0;
static uint64_t vfs_channel = 0;

// Common FAT Boot Sector structure (works for FAT12/16/32)
typedef struct {
    uint8_t jump[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;  // Non-zero for FAT12/16, 0 for FAT32
    uint16_t small_sectors; // Non-zero for FAT12/16, 0 for FAT32
    uint8_t media_descriptor;
    uint16_t fat_size_16; // Non-zero for FAT12/16, 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t large_sectors;

    // Extended Boot Record - different for FAT12/16 vs FAT32
    union {
        // FAT12/16 Extended Boot Record
        struct {
            uint8_t drive_number;
            uint8_t reserved1;
            uint8_t boot_signature;
            uint32_t volume_id;
            char volume_label[11];
            char fs_type[8];
            uint8_t boot_code[448];
        } __attribute__((packed)) fat16;

        // FAT32 Extended Boot Record
        struct {
            uint32_t fat_size_32;
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;
            uint16_t fs_info;
            uint16_t backup_boot;
            uint8_t reserved[12];
            uint8_t drive_number;
            uint8_t reserved1;
            uint8_t boot_signature;
            uint32_t volume_id;
            char volume_label[11];
            char fs_type[8];
            uint8_t boot_code[420];
        } __attribute__((packed)) fat32;
    } ebr;

    uint16_t boot_signature;
} __attribute__((packed)) FATBootSector;

static FATBootSector boot_sector;

static bool read_storage_sectors(uint64_t start_sector, uint32_t sector_count,
                                 void *buffer) {
    if (!storage_driver_channel) {
        printf("FAT32: No storage driver channel available\n");
        return false;
    }

    printf("FAT32: Requesting %u sectors starting at LBA %lu from storage "
           "driver channel %lu\n",
           sector_count, start_sector, storage_driver_channel);

    static char __attribute__((aligned(4096))) io_buffer[4096];
    StorageIOMessage *io_msg = (StorageIOMessage *)io_buffer;

    io_msg->msg_type = STORAGE_MSG_READ_SECTORS;
    io_msg->start_sector = start_sector;
    io_msg->sector_count = sector_count;
    io_msg->reserved = 0;

    size_t msg_size = sizeof(StorageIOMessage);
    printf("FAT32: Sending storage I/O message (size: %lu)\n", msg_size);

    SyscallResult result =
            anos_send_message(storage_driver_channel, 0, msg_size, io_buffer);

    printf("FAT32: Storage I/O syscall result: %ld, value: %lu\n",
           result.result, result.value);

    if (result.result == SYSCALL_OK && result.value > 0) {
        // Use the real data returned by the AHCI driver
        size_t data_returned = result.value;
        size_t expected_size = sector_count * 512;

        printf("FAT32: Received %lu bytes from storage driver (expected %lu)\n",
               data_returned, expected_size);

        // Debug: Show first few bytes of returned data
        printf("FAT32: First 16 bytes of returned data: ");
        uint8_t *data_bytes = (uint8_t *)io_buffer;
        for (int i = 0; i < 16 && i < data_returned; i++) {
            printf("%02x ", data_bytes[i]);
        }
        printf("\n");

        if (data_returned >= expected_size) {
            // Copy the real sector data from the AHCI driver response
            memcpy(buffer, io_buffer, expected_size);
            printf("FAT32: Successfully read real sector data from storage\n");
            return true;
        } else {
            printf("FAT32: Storage driver returned insufficient data (%lu < "
                   "%lu)\n",
                   data_returned, expected_size);
            return false;
        }
    } else {
        printf("FAT32: Storage I/O request failed - result: %ld, value: %lu\n",
               result.result, result.value);
    }

    return false;
}

static bool initialize_fat32_filesystem() {
    // Read the boot sector
    if (!read_storage_sectors(0, 1, &boot_sector)) {
        printf("FAT32: Failed to read boot sector\n");
        return false;
    }

    // Debug: Dump first 16 bytes of boot sector
    printf("FAT32: Boot sector first 16 bytes: ");
    uint8_t *boot_data = (uint8_t *)&boot_sector;
    for (int i = 0; i < 16; i++) {
        printf("%02x ", boot_data[i]);
    }
    printf("\n");

    // Determine FAT type and show fs_type field
    const char *fs_type_field;
    if (boot_sector.fat_size_16 == 0) {
        // FAT32
        fs_type_field = boot_sector.ebr.fat32.fs_type;
        printf("FAT: Detected FAT32 filesystem\n");
    } else {
        // FAT12/16
        fs_type_field = boot_sector.ebr.fat16.fs_type;
        printf("FAT: Detected FAT12/16 filesystem\n");
    }

    printf("FAT: fs_type field: '");
    for (int i = 0; i < 8; i++) {
        if (fs_type_field[i] >= 32 && fs_type_field[i] <= 126) {
            printf("%c", fs_type_field[i]);
        } else {
            printf("\\x%02x", fs_type_field[i]);
        }
    }
    printf("'\n");

    // Basic validation - support FAT12, FAT16, and FAT32
    if (strncmp(fs_type_field, "FAT12", 5) != 0 &&
        strncmp(fs_type_field, "FAT16", 5) != 0 &&
        strncmp(fs_type_field, "FAT32", 5) != 0) {
        printf("FAT: Unsupported filesystem type (not FAT12/16/32)\n");
        return false;
    }

    printf("FAT: Filesystem initialized\n");
    printf("  Bytes per sector: %u\n", boot_sector.bytes_per_sector);
    printf("  Sectors per cluster: %u\n", boot_sector.sectors_per_cluster);

    if (boot_sector.fat_size_16 == 0) {
        // FAT32
        printf("  FAT size: %u sectors\n", boot_sector.ebr.fat32.fat_size_32);
        printf("  Root cluster: %u\n", boot_sector.ebr.fat32.root_cluster);
    } else {
        // FAT12/16
        printf("  FAT size: %u sectors\n", boot_sector.fat_size_16);
        printf("  Root entries: %u\n", boot_sector.root_entries);
    }

    return true;
}

static void handle_filesystem_message(uint64_t msg_cookie, uint64_t tag,
                                      void *buffer, size_t buffer_size) {
    uint64_t result = 0;

    switch (tag) {
    case FS_MSG_QUERY_OBJECT_SIZE: {
        FilesystemObjectSizeQuery *query = (FilesystemObjectSizeQuery *)buffer;

        static char __attribute__((aligned(4096))) response_buffer[4096];
        FilesystemObjectSizeResponse *response =
                (FilesystemObjectSizeResponse *)response_buffer;

        printf("FAT32: Query object size for: %s\n", query->path);

        // TODO: Implement actual FAT32 file lookup
        // For now, return error indicating file operations not yet implemented
        response->object_size = 0;
        response->error_code = -1; // Not implemented
        response->reserved = 0;

        anos_reply_message(msg_cookie, sizeof(FilesystemObjectSizeResponse));
        return;
    }

    case FS_MSG_LOAD_OBJECT_PAGE: {
        FilesystemObjectPageLoad *load_req = (FilesystemObjectPageLoad *)buffer;

        static char __attribute__((aligned(4096))) response_buffer[4096];
        FilesystemObjectPageResponse *response =
                (FilesystemObjectPageResponse *)response_buffer;

        printf("FAT32: Load object page for: %s (offset: %lu, pages: %u)\n",
               load_req->path, load_req->page_offset, load_req->page_count);

        // TODO: Implement actual FAT32 file reading using real sector data
        // For now, return error indicating file operations not yet implemented
        response->pages_loaded = 0;
        response->error_code = -1; // Not implemented

        anos_reply_message(msg_cookie, sizeof(FilesystemObjectPageResponse));
        return;
    }

    default:
        printf("FAT32: Unknown filesystem message type: %lu\n", tag);
        break;
    }

    anos_reply_message(msg_cookie, result);
}

static bool register_with_vfs(const char *mount_prefix) {
    // Find VFS channel
    SyscallResult vfs_result = anos_find_named_channel("SYSTEM::VFS");
    if (vfs_result.result != SYSCALL_OK || vfs_result.value == 0) {
        printf("FAT32: Failed to find SYSTEM::VFS channel\n");
        return false;
    }

    vfs_channel = vfs_result.value;

    // Register filesystem driver with VFS
    static char __attribute__((aligned(4096))) reg_buffer[4096];
    VFSMountEntry *mount_entry = (VFSMountEntry *)reg_buffer;

    strncpy(mount_entry->mount_prefix, mount_prefix,
            sizeof(mount_entry->mount_prefix) - 1);
    mount_entry->mount_prefix[sizeof(mount_entry->mount_prefix) - 1] = '\0';
    mount_entry->fs_driver_channel = fat32_channel;

    SyscallResult reg_result =
            anos_send_message(vfs_channel, VFS_REGISTER_FILESYSTEM,
                              sizeof(VFSMountEntry), reg_buffer);

    if (reg_result.result == SYSCALL_OK && reg_result.value > 0) {
        printf("FAT32: Successfully registered with VFS for mount prefix "
               "'%s'\n",
               mount_prefix);
        return true;
    }

    printf("FAT32: Failed to register with VFS\n");
    return false;
}

static uint32_t query_storage_devices_fat32(uint64_t devman_channel,
                                            DeviceInfo *device_info_out) {
    static char __attribute__((aligned(4096))) query_buffer[4096];
    DeviceQueryMessage *query = (DeviceQueryMessage *)query_buffer;

    query->msg_type = DEVICE_MSG_QUERY;
    query->query_type = QUERY_BY_TYPE;
    query->device_type = DEVICE_TYPE_STORAGE;
    query->device_id = 0;

    SyscallResult query_result = anos_send_message(
            devman_channel, 0, sizeof(DeviceQueryMessage), query_buffer);

    if (query_result.result == SYSCALL_OK && query_result.value > 0) {
        // Check if we got a structured response with device info
        if (query_result.value > sizeof(DeviceQueryResponse)) {
            DeviceQueryResponse *response = (DeviceQueryResponse *)query_buffer;
            if (response->device_count > 0 && response->error_code == 0) {
                // Copy the first device info
                DeviceInfo *devices = (DeviceInfo *)response->data;
                if (device_info_out) {
                    *device_info_out = devices[0];
                }
                return response->device_count;
            }
        }
        // Fallback: old-style response with just count
        return (uint32_t)query_result.value;
    }

    return 0;
}

static bool find_and_connect_to_storage_device() {
    printf("FAT32: Waiting for DEVMAN to become available...\n");

    // Wait for DEVMAN to be available
    uint64_t devman_channel = 0;
    for (int retry = 0; retry < 10; retry++) {
        SyscallResult devman_result = anos_find_named_channel("DEVMAN");
        if (devman_result.result == SYSCALL_OK && devman_result.value != 0) {
            devman_channel = devman_result.value;
            printf("FAT32: DEVMAN found, waiting for storage device "
                   "discovery...\n");
            break;
        }
        printf("FAT32: DEVMAN not found yet, waiting... (attempt %d/10)\n",
               retry + 1);
        anos_task_sleep_current_secs(1);
    }

    if (!devman_channel) {
        printf("FAT32: DEVMAN not available after waiting, cannot continue\n");
        return false;
    }

    // Wait for storage devices to be discovered
    uint32_t storage_device_count = 0;
    DeviceInfo storage_device_info = {0};
    printf("FAT32: Polling for storage device discovery...\n");

    for (int retry = 0; retry < 20; retry++) {
        storage_device_count = query_storage_devices_fat32(
                devman_channel, &storage_device_info);
        if (storage_device_count > 0) {
            printf("FAT32: Found %u storage devices after %d seconds\n",
                   storage_device_count, retry + 1);
            break;
        }
        printf("FAT32: No storage devices found yet, waiting... (attempt "
               "%d/20)\n",
               retry + 1);
        anos_task_sleep_current_secs(1);
    }

    if (storage_device_count > 0) {
        // Use the first storage device found and get its driver channel
        storage_device_id = storage_device_info.device_id;
        storage_driver_channel = storage_device_info.driver_channel;

        printf("FAT32: Using storage device '%s' (ID: %lu, Channel: %lu)\n",
               storage_device_info.name, storage_device_id,
               storage_driver_channel);
        printf("FAT32: Storage driver: %s\n", storage_device_info.driver_name);
        return true;
    }

    printf("FAT32: No storage devices discovered after waiting 20 seconds\n");
    return false;
}

int main(int argc, char **argv) {
    printf("\nFAT32 Filesystem Driver #%s [libanos #%s]\n", VERSION,
           libanos_version());

    if (argc < 2) {
        printf("Usage: %s <mount_prefix>\n", argv[0]);
        printf("Example: %s disk0:\n", argv[0]);
        return 1;
    }

    const char *mount_prefix = argv[1];
    printf("FAT32: Starting for mount prefix '%s'\n", mount_prefix);

    // Create IPC channel for this filesystem driver
    SyscallResult channel_result = anos_create_channel();
    if (channel_result.result != SYSCALL_OK) {
        printf("FAT32: Failed to create IPC channel\n");
        return 1;
    }

    fat32_channel = channel_result.value;
    printf("FAT32: Created IPC channel %lu\n", fat32_channel);

    // Find and connect to storage device
    if (!find_and_connect_to_storage_device()) {
        printf("FAT32: Failed to find storage device\n");
        return 1;
    }

    // Initialize filesystem
    if (!initialize_fat32_filesystem()) {
        printf("FAT32: Failed to initialize filesystem\n");
        return 1;
    }

    // Register with VFS
    if (!register_with_vfs(mount_prefix)) {
        printf("FAT32: Failed to register with VFS\n");
        return 1;
    }

    printf("FAT32: Filesystem driver initialized and ready\n");

    // Main message loop
    while (1) {
        static char __attribute__((aligned(4096))) ipc_buffer[4096];
        size_t buffer_size = sizeof(ipc_buffer);
        uint64_t tag = 0;

        SyscallResult recv_result = anos_recv_message(fat32_channel, &tag,
                                                      &buffer_size, ipc_buffer);
        uint64_t msg_cookie = recv_result.value;

        if (recv_result.result == SYSCALL_OK && msg_cookie) {
            handle_filesystem_message(msg_cookie, tag, ipc_buffer, buffer_size);
        } else {
            printf("FAT32: Error receiving message\n");
            anos_task_sleep_current_secs(1);
        }
    }

    return 0;
}