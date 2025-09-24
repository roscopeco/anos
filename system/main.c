/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <anos/syscalls.h>
#include <anos/types.h>

#include "../servers/common/device_types.h"
#include "../servers/common/filesystem_types.h"
#include "path.h"
#include "process.h"
#include "ramfs.h"

#include "config.h"
#include "jansson.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#ifdef DEBUG_FS_INIT
#define fs_debugf printf
#else
#define fs_debugf(...)
#endif

#define FS_QUERY_OBJECT_SIZE ((1))
#define FS_LOAD_OBJECT_PAGE ((2))

#define PROCESS_SPAWN ((1))

#define VM_PAGE_SIZE ((0x1000))
#define DRIVER_THREAD_STACK_PAGES ((0x10))
#define DRIVER_THREAD_STACK_SIZE ((VM_PAGE_SIZE * DRIVER_THREAD_STACK_PAGES))

typedef struct {
    uint64_t start_byte_ofs;
    char name[];
} FileLoadPageQuery;

typedef struct {
    uint64_t stack_size;
    uint16_t argc;
    uint16_t capc;
    char data[];
} ProcessSpawnRequest;

extern AnosRAMFSHeader _system_ramfs_start;

extern uint64_t __syscall_capabilities[];

static char __attribute__((aligned(VM_PAGE_SIZE))) ramfs_driver_thread_stack[DRIVER_THREAD_STACK_SIZE];
static char __attribute__((aligned(VM_PAGE_SIZE))) process_manager_thread_stack[DRIVER_THREAD_STACK_SIZE];
static char __attribute__((aligned(VM_PAGE_SIZE))) filesystem_starter_thread_stack[DRIVER_THREAD_STACK_SIZE];

#ifdef TEST_THREAD_KILL
static char __attribute__((aligned(VM_PAGE_SIZE))) kamikaze_thread_stack[VM_PAGE_SIZE];
#endif

static uint64_t ramfs_channel;
static uint64_t process_manager_channel;

// Filesystem registry for VFS
#define MAX_VFS_FILESYSTEMS 32
static VFSMountEntry filesystem_mounts[MAX_VFS_FILESYSTEMS];
static uint32_t filesystem_mount_count = 0;

// Global capabilities for filesystem drivers
static constexpr uint16_t global_fs_cap_count = 17;
static InitCapability global_fs_caps[global_fs_cap_count];

static inline void banner() {
    printf("\n\nSYSTEM User-mode Supervisor #%s [libanos #%s]\n", VERSION, libanos_version());
}

#ifdef DEBUG_INIT_RAMFS
static void dump_fs(AnosRAMFSHeader *ramfs) {
    AnosRAMFSFileHeader *hdr = (AnosRAMFSFileHeader *)(ramfs + 1);

    printf("System FS magic: 0x%08x\n", ramfs->magic);
    printf("System FS ver  : 0x%08x\n", ramfs->version);
    printf("System FS count: 0x%016lx\n", ramfs->file_count);
    printf("System FS size : 0x%016lx\n", ramfs->fs_size);

    for (int i = 0; i < ramfs->file_count; i++) {
        if (!hdr) {
            printf("dump_fs: Could not find file %s\n", hdr->file_name);
            return;
        }

        uint8_t *fbuf = ramfs_file_open(hdr);

        if (!fbuf) {
            printf("dump_fs: Could not open file %s\n", hdr->file_name);
            return;
        }

        printf("%-20s [%10ld]: ", hdr->file_name, hdr->file_length);
        for (int i = 0; i < 16; i++) {
            printf("0x%02x ", *fbuf++);
        }
        printf(" ... \n");

        hdr++;
    }
}
#else
#define dump_fs(...)
#endif

static void init_global_fs_caps() {
    const InitCapability new_process_caps[] = {
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT],
                    .capability_id = SYSCALL_ID_DEBUG_PRINT,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR],
                    .capability_id = SYSCALL_ID_DEBUG_CHAR,
            },
            // N.B! malloc (via sbrk) in the stdlib needs create region cap!
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_CREATE_REGION],
                    .capability_id = SYSCALL_ID_CREATE_REGION,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_SLEEP],
                    .capability_id = SYSCALL_ID_SLEEP,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_FIRMWARE_TABLES],
                    .capability_id = SYSCALL_ID_MAP_FIRMWARE_TABLES,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL],
                    .capability_id = SYSCALL_ID_MAP_PHYSICAL,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL],
                    .capability_id = SYSCALL_ID_MAP_VIRTUAL,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_ALLOC_PHYSICAL_PAGES],
                    .capability_id = SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE],
                    .capability_id = SYSCALL_ID_SEND_MESSAGE,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL],
                    .capability_id = SYSCALL_ID_FIND_NAMED_CHANNEL,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK],
                    .capability_id = SYSCALL_ID_KILL_CURRENT_TASK,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_ALLOC_INTERRUPT_VECTOR],
                    .capability_id = SYSCALL_ID_ALLOC_INTERRUPT_VECTOR,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_WAIT_INTERRUPT],
                    .capability_id = SYSCALL_ID_WAIT_INTERRUPT,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE],
                    .capability_id = SYSCALL_ID_RECV_MESSAGE,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE],
                    .capability_id = SYSCALL_ID_REPLY_MESSAGE,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL],
                    .capability_id = SYSCALL_ID_CREATE_CHANNEL,
            },
            {
                    .capability_cookie = __syscall_capabilities[SYSCALL_ID_REGISTER_NAMED_CHANNEL],
                    .capability_id = SYSCALL_ID_REGISTER_NAMED_CHANNEL,
            },
    };

    memcpy((void *)global_fs_caps, new_process_caps, sizeof(InitCapability) * global_fs_cap_count);
}

static void handle_file_size_query(const uint64_t message_cookie, const size_t message_size, char *message_buffer) {
    char *mount_point, *path;

    // Refuse oversized messages...
    // TODO the kernel should guarantee this!
    if (message_size > MAX_IPC_BUFFER_SIZE) {
        anos_reply_message(message_cookie, 0);
    }

    if (parse_file_path(message_buffer, message_size, &mount_point, &path)) {
        // ignore leading slashes
        while (*path == '/') {
            path++;
        }

        const AnosRAMFSFileHeader *target = ramfs_find_file(&_system_ramfs_start, path);

        if (target) {
#ifdef DEBUG_SYS_IPC
            printf("    -> %s found: %ld byte(s)\n", target->file_name, target->file_length);
#endif
            anos_reply_message(message_cookie, target->file_length);
        } else {
#ifdef DEBUG_SYS_IPC
            printf("    -> %s not found\n", path);
#endif
            anos_reply_message(message_cookie, 0);
        }
    } else {
        // Bad message content
        anos_reply_message(message_cookie, 0);
    }
}

static void handle_file_load_page_query(const uint64_t message_cookie, const size_t message_size,
                                        void *message_buffer) {
    char *mount_point, *path;
    FileLoadPageQuery *pq = message_buffer;

    // Refuse oversized messages...
    if (message_size > MAX_IPC_BUFFER_SIZE) {
        anos_reply_message(message_cookie, 0);
    }

    if (parse_file_path(pq->name, message_size - 4, &mount_point, &path)) {
        // ignore leading slashes
        while (*path == '/') {
            path++;
        }

        AnosRAMFSFileHeader *target = ramfs_find_file((AnosRAMFSHeader *)&_system_ramfs_start, path);

        if (target) {
            // figure out size to copy
            size_t copy_size = target->file_length - pq->start_byte_ofs;
            if (copy_size > MAX_IPC_BUFFER_SIZE) {
                copy_size = MAX_IPC_BUFFER_SIZE;
            }

            const uint8_t *src = ramfs_file_open(target);
            src += pq->start_byte_ofs;

            uint8_t *dest = message_buffer;

            for (int i = 0; i < copy_size; i++) {
                *dest++ = *src++;
            }

            anos_reply_message(message_cookie, copy_size);
        } else {
            anos_reply_message(message_cookie, 0);
        }
    } else {
        // Bad message content
        anos_reply_message(message_cookie, 0);
    }
}

static void handle_process_spawn_request(const uint64_t message_cookie, const size_t message_size,
                                         void *message_buffer) {

    ProcessSpawnRequest *spawn_req = (ProcessSpawnRequest *)message_buffer;

    if (message_size < sizeof(ProcessSpawnRequest) || !spawn_req) {
        anos_reply_message(message_cookie, -1);
        return;
    }

    const uint16_t argc = spawn_req->argc;
    const uint16_t capc = spawn_req->capc;
    const uint64_t stack_size = spawn_req->stack_size;

    if (argc > 64 || capc > 32) {
        anos_reply_message(message_cookie, -10);
        return;
    }

    char *data_ptr = spawn_req->data;
    size_t remaining_size = message_size - sizeof(ProcessSpawnRequest);

    static InitCapability capabilities[32];
    static const char *argv[64];

    if (capc > 0) {
        if (remaining_size < capc * sizeof(InitCapability)) {
            anos_reply_message(message_cookie, -2);
            return;
        }

        memcpy(capabilities, data_ptr, capc * sizeof(InitCapability));
        data_ptr += capc * sizeof(InitCapability);
        remaining_size -= capc * sizeof(InitCapability);
    }

    if (argc > 0) {
        const char *str_data = data_ptr;
        size_t str_offset = 0;

        for (uint16_t i = 0; i < argc; i++) {
            if (str_offset >= remaining_size) {
                anos_reply_message(message_cookie, -3);
                return;
            }

            argv[i] = str_data + str_offset;

            while (str_offset < remaining_size && str_data[str_offset] != '\0') {
                str_offset++;
            }

            if (str_offset >= remaining_size) {
                anos_reply_message(message_cookie, -4);
                return;
            }

            str_offset++;
        }
    }

#ifdef DEBUG_SYS_IPC
    printf("SYSTEM::PROCESS spawning process with stack_size=%lu, argc=%u, "
           "capc=%u\n",
           stack_size, argc, capc);
    if (argc > 0) {
        printf("  -> executable: %s\n", argv[0]);
    }
#endif

    const int64_t pid =
            create_server_process(stack_size, capc, capc > 0 ? capabilities : nullptr, argc, argc > 0 ? argv : nullptr);

    anos_reply_message(message_cookie, pid);
}

static noreturn void process_manager_thread(void) {
    while (true) {
        uint64_t tag;
        size_t message_size;
        char *message_buffer = (char *)0xc0000000;

        const SyscallResult result = anos_recv_message(process_manager_channel, &tag, &message_size, message_buffer);

        uint64_t message_cookie = result.value;

        if (result.result == SYSCALL_OK && message_cookie) {
#ifdef DEBUG_SYS_IPC
            printf("SYSTEM::PROCESS received [0x%016lx] 0x%016lx (%ld bytes)\n", message_cookie, tag, message_size);
#endif
            switch (tag) {
            case PROCESS_SPAWN:
                handle_process_spawn_request(message_cookie, message_size, message_buffer);
                break;
            default:
#ifdef DEBUG_SYS_IPC
                printf("WARN: Unhandled message [tag 0x%016lx] to "
                       "SYSTEM::PROCESS\n",
                       tag);
#endif
                anos_reply_message(message_cookie, -999);
                break;
            }
#ifdef CONSERVATIVE_BUILD
        } else {
            printf("WARN: NULL message cookie in process manager\n");
#endif
        }
    }
}

static noreturn void ramfs_driver_thread(void) {
    while (true) {
        uint64_t tag;
        size_t message_size;
        char *message_buffer = (char *)0xb0000000;

        const SyscallResult result = anos_recv_message(ramfs_channel, &tag, &message_size, message_buffer);

        uint64_t message_cookie = result.value;

        if (result.result == SYSCALL_OK && message_cookie) {
#ifdef DEBUG_SYS_IPC
            printf("SYSTEM::<ramfs> received [0x%016lx] 0x%016lx (%ld "
                   "bytes): %s\n",
                   message_cookie, tag, message_size, message_buffer);
#endif
            switch (tag) {
            case FS_QUERY_OBJECT_SIZE:
                handle_file_size_query(message_cookie, message_size, message_buffer);
                break;
            case FS_LOAD_OBJECT_PAGE:
                handle_file_load_page_query(message_cookie, message_size, message_buffer);
                break;
            default:
#ifdef DEBUG_SYS_IPC
                printf("WARN: Unhandled message [tag 0x%016lx] to "
                       "SYSTEM::<ramfs>\n",
                       tag);
#endif
                anos_reply_message(message_cookie, 0);
                break;
            }
#ifdef CONSERVATIVE_BUILD
        } else {
            printf("WARN: NULL message cookie\n");
#endif
        }
    }
}

static void start_filesystem_drivers(const InitCapability *caps, uint16_t cap_count);

static noreturn void filesystem_starter_thread(void) {
    // TODO this isn't very fucking robust...
    // Wait a bit for the system to stabilize
    anos_task_sleep_current_secs(2);

    start_filesystem_drivers(global_fs_caps, global_fs_cap_count);

    // TODO this thread should terminate, but that causes #PF right now...
    while (true) {
        anos_task_sleep_current_secs(3600);
    }
}

#ifdef TEST_THREAD_KILL
static void kamikaze_thread(void) {
    printf("Kamikaze thread must die!!\n");
    anos_kill_current_task();
}
#endif

// VFS filesystem management functions
static bool register_filesystem_driver(const char *mount_prefix, const uint64_t fs_channel) {

    if (filesystem_mount_count >= MAX_VFS_FILESYSTEMS) {
        return false; // Registry full
    }

    // Check if mount prefix already exists
    for (uint32_t i = 0; i < filesystem_mount_count; i++) {
        if (strncmp(filesystem_mounts[i].mount_prefix, mount_prefix, sizeof(filesystem_mounts[i].mount_prefix)) == 0) {
            return false; // Mount prefix already registered
        }
    }

    strncpy(filesystem_mounts[filesystem_mount_count].mount_prefix, mount_prefix,
            sizeof(filesystem_mounts[filesystem_mount_count].mount_prefix) - 1);
    filesystem_mounts[filesystem_mount_count]
            .mount_prefix[sizeof(filesystem_mounts[filesystem_mount_count].mount_prefix) - 1] = '\0';
    filesystem_mounts[filesystem_mount_count].fs_driver_channel = fs_channel;
    filesystem_mount_count++;

    printf("VFS: Registered filesystem driver for mount prefix '%s'", mount_prefix);
    fs_debugf(" (channel: 0x%016lx)", fs_channel);
    printf("\n");

    return true;
}

static uint64_t find_filesystem_driver_by_path(const char *path) {
    // Extract mount prefix from path (everything up to the first colon)
    const char *colon_pos = strchr(path, ':');
    if (!colon_pos) {
        return 0; // No mount prefix in path
    }

    const size_t prefix_len = colon_pos - path + 1; // Include the colon
    char mount_prefix[32];

    if (prefix_len >= sizeof(mount_prefix)) {
        return 0; // Mount prefix too long
    }

    strncpy(mount_prefix, path, prefix_len);
    mount_prefix[prefix_len] = '\0';

    // Find filesystem driver for this mount prefix
    for (uint32_t i = 0; i < filesystem_mount_count; i++) {
        if (strncmp(filesystem_mounts[i].mount_prefix, mount_prefix, sizeof(filesystem_mounts[i].mount_prefix)) == 0) {
            return filesystem_mounts[i].fs_driver_channel;
        }
    }

    return 0; // No filesystem driver found
}

static bool spawn_filesystem_driver(const char *driver_name, const char *mount_prefix, const InitCapability *caps,
                                    const uint16_t cap_count) {
    char driver_path[64];
    snprintf(driver_path, sizeof(driver_path), "boot:/%s.elf", driver_name);

    const char *fs_argv[] = {driver_path, mount_prefix};
    const int64_t fs_pid = create_server_process(0x100000, cap_count, caps, 2, fs_argv);

    if (fs_pid < 0) {
        fs_debugf("Warning: Failed to create %s filesystem driver for %s\n", driver_name, mount_prefix);
        return false;
    }

    fs_debugf("Started %s filesystem driver (PID: %ld) for mount: %s\n", driver_name, fs_pid, mount_prefix);
    return true;
}

static uint32_t query_storage_devices(const uint64_t devman_channel, DeviceInfo *devices_out, uint32_t max_devices) {

    static char __attribute__((aligned(4096))) query_buffer[4096];

    DeviceQueryMessage *query = (DeviceQueryMessage *)query_buffer;

    query->msg_type = DEVICE_MSG_QUERY;
    query->query_type = QUERY_BY_TYPE;
    query->device_type = DEVICE_TYPE_STORAGE;
    query->device_id = 0;

    const SyscallResult query_result = anos_send_message(devman_channel, 0, sizeof(DeviceQueryMessage), query_buffer);

    if (query_result.result == SYSCALL_OK && query_result.value > 0) {
        fs_debugf("DEBUG: DEVMAN query returned %lu bytes\n", query_result.value);

        // Check if we got a structured response with device info
        if (query_result.value > sizeof(DeviceQueryResponse)) {

            const DeviceQueryResponse *response = (DeviceQueryResponse *)query_buffer;

            fs_debugf("DEBUG: Structured response - count=%u, error=%u\n", response->device_count,
                      response->error_code);

            if (response->device_count > 0 && response->error_code == 0) {
                // Copy device info to output array
                const DeviceInfo *devices = (DeviceInfo *)response->data;
                uint32_t count = response->device_count;
                if (count > max_devices)
                    count = max_devices;

                fs_debugf("DEBUG: Copying %u device info structures\n", count);
                if (devices_out) {
                    memcpy(devices_out, devices, count * sizeof(DeviceInfo));
                }
                return count;
            }
        }
    }

    return 0;
}

static void start_filesystem_drivers(const InitCapability *caps, uint16_t cap_count) {
    fs_debugf("Waiting for DEVMAN to become available...\n");

    // Wait for DEVMAN to be available
    uint64_t devman_channel = 0;
    for (int retry = 0; retry < 10; retry++) {

        const SyscallResult devman_result = anos_find_named_channel("DEVMAN");
        if (devman_result.result == SYSCALL_OK && devman_result.value != 0) {
            devman_channel = devman_result.value;

            fs_debugf("DEVMAN found, waiting for device discovery to "
                      "complete...\n");

            break;
        }
        fs_debugf("  DEVMAN not found yet, waiting... (attempt %d/10)\n", retry + 1);
        anos_task_sleep_current_secs(1);
    }

    if (!devman_channel) {
        printf("Warning: DEVMAN not available, no filesystems started\n");
        return;
    }

    // Wait for storage devices to be discovered
    // Poll DEVMAN until we find storage devices or timeout
    static DeviceInfo storage_devices[8]; // Max 8 storage devices
    uint32_t storage_device_count = 0;
    printf("Waiting for storage device discovery...\n");

    for (int retry = 0; retry < 15; retry++) {
        storage_device_count = query_storage_devices(devman_channel, storage_devices, 8);
        if (storage_device_count > 0) {
            fs_debugf("Found %u storage devices after %d seconds\n", storage_device_count, retry + 1);
            break;
        }
        fs_debugf("  No storage devices found yet, waiting... (attempt %d/15)\n", retry + 1);
        anos_task_sleep_current_secs(1);
    }

    if (storage_device_count > 0) {
        fs_debugf("Starting filesystem drivers for %u discovered storage "
                  "devices...\n",
                  storage_device_count);

        for (uint32_t i = 0; i < storage_device_count; i++) {

#ifdef DEBUG_FS_INIT
            DeviceInfo *device = &storage_devices[i];
            fs_debugf("  Device %u: ID=%lu, Name='%s', Driver='%s', Channel=%lu, "
                      "Type=%u, HW=%u, Caps=0x%x\n",
                      i, device->device_id, device->name, device->driver_name, device->driver_channel,
                      device->device_type, device->hardware_type, device->capabilities);
#endif
        }

        // Start filesystem drivers based on real discovered devices
        for (uint32_t i = 0; i < storage_device_count; i++) {
            const DeviceInfo *device = &storage_devices[i];

            // Verify this is a real, functional storage device
            if (device->driver_channel == 0) {
                fs_debugf("  Skipping device '%s' - no driver channel\n", device->name);
                continue;
            }

            if (!(device->capabilities & (DEVICE_CAP_READ | DEVICE_CAP_WRITE))) {
                fs_debugf("  Skipping device '%s' - no read/write capabilities\n", device->name);
                continue;
            }

            // Create a mount prefix based on the device name and type
            char mount_prefix[32];
            if (device->hardware_type == STORAGE_HW_AHCI) {
                snprintf(mount_prefix, sizeof(mount_prefix), "disk%u:", i);
            } else if (device->hardware_type == STORAGE_HW_USB) {
                snprintf(mount_prefix, sizeof(mount_prefix), "usb%u:", i);
            } else if (device->hardware_type == STORAGE_HW_NVME) {
                snprintf(mount_prefix, sizeof(mount_prefix), "nvme%u:", i);
            } else {
                snprintf(mount_prefix, sizeof(mount_prefix), "storage%u:", i);
            }

            fs_debugf("  Starting filesystem driver for device '%s' (ID: %lu, "
                      "Driver: %s) -> %s\n",
                      device->name, device->device_id, device->driver_name, mount_prefix);

            spawn_filesystem_driver("fat32drv", mount_prefix, caps, cap_count);
        }
    } else {
        printf("No functional storage devices discovered after waiting - no "
               "filesystem drivers started\n");
    }
}

int main(int argc, char **argv) {
    banner();

#ifdef TEST_THREAD_KILL
    const SyscallResult create_kill_result =
            anos_create_thread(kamikaze_thread, (uintptr_t)kamikaze_thread_stack + VM_PAGE_SIZE - 8);

    if (create_kill_result.result != SYSCALL_OK) {
        printf("Failed to create kamikaze thread...\n");
    }
#endif

    AnosMemInfo meminfo;
    if (anos_get_mem_info(&meminfo).result == SYSCALL_OK) {
        printf("%ld / %ld bytes used / free\n", meminfo.physical_total - meminfo.physical_avail,
               meminfo.physical_avail);
    } else {
        printf("WARN: Get mem info failed\n");
    }

#ifdef DEBUG_INIT_RAMFS
    AnosRAMFSHeader *ramfs = (AnosRAMFSHeader *)&_system_ramfs_start;
    dump_fs(ramfs);
#endif

    const SyscallResult create_vfs_result = anos_create_channel();
    const uint64_t vfs_channel = create_vfs_result.value;

    const SyscallResult create_ramfs_result = anos_create_channel();
    ramfs_channel = create_ramfs_result.value;

    const SyscallResult create_process_manager_result = anos_create_channel();
    process_manager_channel = create_process_manager_result.value;

    const bool all_created_ok = create_vfs_result.result == SYSCALL_OK && create_ramfs_result.result == SYSCALL_OK &&
                                create_process_manager_result.result == SYSCALL_OK;

    if (all_created_ok && vfs_channel && ramfs_channel && process_manager_channel) {
        if (anos_register_channel_name(vfs_channel, "SYSTEM::VFS").result == SYSCALL_OK) {
            if (anos_register_channel_name(process_manager_channel, "SYSTEM::PROCESS").result != SYSCALL_OK) {
                printf("Failed to register SYSTEM::PROCESS channel!\n");
            }
            // set up RAMFS driver thread
            SyscallResult create_thread_result = anos_create_thread(
                    ramfs_driver_thread, (uintptr_t)ramfs_driver_thread_stack + DRIVER_THREAD_STACK_SIZE - 8);

            if (create_thread_result.result != SYSCALL_OK) {
                printf("Failed to create RAMFS driver thread!\n");
            } else {
                // Register the built-in ramfs for boot: mount prefix
                register_filesystem_driver("boot:", ramfs_channel);
            }

            create_thread_result = anos_create_thread(process_manager_thread, (uintptr_t)process_manager_thread_stack +
                                                                                      DRIVER_THREAD_STACK_SIZE - 8);

            // set up process manager thread
            if (create_thread_result.result != SYSCALL_OK) {
                printf("Failed to create process manager thread!\n");
            }

            process_config("{  \"boot_servers\": [    {      \"name\": \"Kernel Log Viewer\", "
                           "     \"path\": \"boot:/kterminal.elf\", \"stack_size\": 2097152,     \"capabilities\": [  "
                           "      \"SYSCALL_DEBUG_PRINT\",        \"SYSCALL_DEBUG_CHAR\",     "
                           "   \"SYSCALL_CREATE_REGION\",        \"SYSCALL_SLEEP\",        "
                           "\"SYSCALL_MAP_FIRMWARE_TABLES\",        \"SYSCALL_MAP_PHYSICAL\", "
                           "       \"SYSCALL_MAP_VIRTUAL\",        "
                           "\"SYSCALL_ALLOC_PHYSICAL_PAGES\",        "
                           "\"SYSCALL_SEND_MESSAGE\",        \"SYSCALL_FIND_NAMED_CHANNEL\",  "
                           "      \"SYSCALL_KILL_CURRENT_TASK\",        "
                           "\"SYSCALL_ALLOC_INTERRUPT_VECTOR\",        "
                           "\"SYSCALL_WAIT_INTERRUPT\",        \"SYSCALL_RECV_MESSAGE\",      "
                           "  \"SYSCALL_REPLY_MESSAGE\",        \"SYSCALL_CREATE_CHANNEL\",   "
                           "     \"SYSCALL_REGISTER_NAMED_CHANNEL\",        "
                           "\"SYSCALL_READ_KERNEL_LOG\",        "
                           "\"SYSCALL_GET_FRAMEBUFFER_PHYS\"      ]    },    {      \"name\": "
                           "\"DEVMAN\",   \"stack_size\": 2097152,   \"path\": \"boot:/devman.elf\",      "
                           "\"capabilities\": [        \"SYSCALL_DEBUG_PRINT\",        "
                           "\"SYSCALL_DEBUG_CHAR\",        \"SYSCALL_CREATE_REGION\",        "
                           "\"SYSCALL_SLEEP\",        \"SYSCALL_MAP_FIRMWARE_TABLES\",        "
                           "\"SYSCALL_MAP_PHYSICAL\",        \"SYSCALL_MAP_VIRTUAL\",        "
                           "\"SYSCALL_ALLOC_PHYSICAL_PAGES\",        "
                           "\"SYSCALL_SEND_MESSAGE\",        \"SYSCALL_FIND_NAMED_CHANNEL\",  "
                           "      \"SYSCALL_KILL_CURRENT_TASK\",        "
                           "\"SYSCALL_ALLOC_INTERRUPT_VECTOR\",        "
                           "\"SYSCALL_WAIT_INTERRUPT\",        \"SYSCALL_RECV_MESSAGE\",      "
                           "  \"SYSCALL_REPLY_MESSAGE\",        \"SYSCALL_CREATE_CHANNEL\",   "
                           "     \"SYSCALL_REGISTER_NAMED_CHANNEL\"      ]    },    {      "
                           "\"name\": \"Test Server\",   \"stack_size\": 2097152,   \"path\": "
                           "\"boot:/test_server.elf\",      \"capabilities\": [        "
                           "\"SYSCALL_DEBUG_PRINT\",        \"SYSCALL_DEBUG_CHAR\",        "
                           "\"SYSCALL_CREATE_REGION\",        \"SYSCALL_SLEEP\"      ], "
                           "\"arguments\": [ \"Hello, World!\" ]    }  ]}");

            // Store capabilities for filesystem drivers
            // TODO get rid of this, and the caps we keep in here, once FS is in config as well...
            init_global_fs_caps();

            // Start filesystem starter thread
            // TODO get rid of this too, SYSTEM has no business starting filesystems directly...
            const SyscallResult fs_thread_result =
                    anos_create_thread(filesystem_starter_thread,
                                       (uintptr_t)filesystem_starter_thread_stack + DRIVER_THREAD_STACK_SIZE - 8);

            if (fs_thread_result.result != SYSCALL_OK) {
                printf("Failed to create filesystem starter thread!\n");
            } else {
                fs_debugf("Filesystem starter thread created\n");
            }

            while (true) {
                uint64_t tag;
                size_t message_size;
                char *message_buffer = (char *)0xa0000000;

                const SyscallResult result = anos_recv_message(vfs_channel, &tag, &message_size, message_buffer);

                const uint64_t message_cookie = result.value;

                if (result.result == SYSCALL_OK && message_cookie) {
#ifdef DEBUG_SYS_IPC
                    printf("SYSTEM::VFS received [0x%016lx] 0x%016lx (%ld "
                           "bytes): %s\n",
                           message_cookie, tag, message_size, message_buffer);
#endif
                    switch (tag) {
                    case VFS_FIND_FS_DRIVER: {
                        // find FS driver by path
                        const char *path = (const char *)message_buffer;
                        uint64_t fs_channel = find_filesystem_driver_by_path(path);
                        anos_reply_message(message_cookie, fs_channel);
                        break;
                    }
                    case VFS_REGISTER_FILESYSTEM: {
                        // Register a filesystem driver
                        if (message_size >= sizeof(VFSMountEntry)) {
                            const VFSMountEntry *mount_entry = (const VFSMountEntry *)message_buffer;
                            bool success = register_filesystem_driver(mount_entry->mount_prefix,
                                                                      mount_entry->fs_driver_channel);
                            anos_reply_message(message_cookie, success ? 1 : 0);
                        } else {
                            anos_reply_message(message_cookie, 0);
                        }
                        break;
                    }
                    default:
#ifdef DEBUG_SYS_IPC
                        printf("WARN: Unhandled message [tag 0x%016lx] to "
                               "SYSTEM::VFS\n",
                               tag);
#endif
                        anos_reply_message(message_cookie, 0);
                        break;
                    }
#ifdef CONSERVATIVE_BUILD
                } else {
                    printf("WARN: NULL message cookie\n");
#endif
                }
            }
        }
    }

    printf("Failed to create SYSTEM:: channels!\n");

    while (true) {
        anos_task_sleep_current_secs(5);
    }
}