/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include <anos/syscalls.h>
#include <anos/types.h>

#include "path.h"
#include "printf.h"
#include "ramfs.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

#define VFS_FIND_FS_DRIVER ((1))

#define FS_QUERY_OBJECT_SIZE ((1))
#define FS_LOAD_OBJECT_PAGE ((2))

extern AnosRAMFSHeader _system_ramfs_start;

static char __attribute__((aligned(0x1000))) ramfs_driver_thread_stack[0x1000];
static char __attribute__((aligned(0x1000))) kamikaze_thread_stack[0x1000];

static uint64_t ramfs_channel;

static inline void banner() {
    printf("\n\nSYSTEM User-mode Supervisor #%s [libanos #%s]\n", VERSION,
           libanos_version());
}

noreturn int initial_server_loader(void);

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

static void handle_file_size_query(uint64_t message_cookie, size_t message_size,
                                   char *message_buffer) {
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

        AnosRAMFSFileHeader *target =
                ramfs_find_file((AnosRAMFSHeader *)&_system_ramfs_start, path);

        if (target) {
            anos_reply_message(message_cookie, target->file_length);
        } else {
            anos_reply_message(message_cookie, 0);
        }
    } else {
        // Bad message content
        anos_reply_message(message_cookie, 0);
    }
}

typedef struct {
    uint64_t start_byte_ofs;
    char name[];
} FileLoadPageQuery;

static void handle_file_load_page_query(uint64_t message_cookie,
                                        size_t message_size,
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

        AnosRAMFSFileHeader *target =
                ramfs_find_file((AnosRAMFSHeader *)&_system_ramfs_start, path);

        if (target) {
            // figure out size to copy
            size_t copy_size = target->file_length - pq->start_byte_ofs;
            if (copy_size > MAX_IPC_BUFFER_SIZE) {
                copy_size = MAX_IPC_BUFFER_SIZE;
            }

            uint8_t *src = ramfs_file_open(target);
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

static void ramfs_driver_thread(void) {
    while (true) {
        uint64_t tag;
        size_t message_size;
        char *message_buffer = (char *)0xb0000000;

        uint64_t message_cookie = anos_recv_message(
                ramfs_channel, &tag, &message_size, message_buffer);

        if (message_cookie) {
#ifdef DEBUG_SYS_IPC
            printf("SYSTEM::VFS received [0x%016lx] 0x%016lx (%ld "
                   "bytes): %s\n",
                   message_cookie, tag, message_size, message_buffer);
#endif
            switch (tag) {
            case FS_QUERY_OBJECT_SIZE:
                handle_file_size_query(message_cookie, message_size,
                                       message_buffer);
                break;
            case FS_LOAD_OBJECT_PAGE:
                handle_file_load_page_query(message_cookie, message_size,
                                            message_buffer);
                break;
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

static void kamikaze_thread(void) {
    printf("Kamikaze thread must die!!\n");
    anos_kill_current_task();
}

int main(int argc, char **argv) {
    banner();

    if (!anos_create_thread(kamikaze_thread,
                            (uintptr_t)kamikaze_thread_stack)) {
        printf("Failed to create kamikaze thread...\n");
    }

    AnosMemInfo meminfo;
    if (anos_get_mem_info(&meminfo) == 0) {
        printf("%ld / %ld bytes used / free\n",
               meminfo.physical_total - meminfo.physical_avail,
               meminfo.physical_avail);
    } else {
        printf("WARN: Get mem info failed\n");
    }

#ifdef DEBUG_INIT_RAMFS
    AnosRAMFSHeader *ramfs = (AnosRAMFSHeader *)&_system_ramfs_start;
    dump_fs(ramfs);
#endif

    ProcessMemoryRegion regions[2] = {
            {
                    .start = 0x1000000,
                    .len_bytes = 0x3000,
            },
            {.start = 0x80000000, .len_bytes = 0x10000}};

    uint64_t new_pid = anos_create_process(0x7ffff000, 0x1000, 2, regions,
                                           (uintptr_t)initial_server_loader);
    if (new_pid < 0) {
        printf("Failed to create new process\n");
    }

    uint64_t vfs_channel = anos_create_channel();
    ramfs_channel = anos_create_channel();

    if (vfs_channel && ramfs_channel) {
        if (anos_register_channel_name(vfs_channel, "SYSTEM::VFS") == 0) {
            // set up RAMFS driver thread
            if (!anos_create_thread(ramfs_driver_thread,
                                    (uintptr_t)ramfs_driver_thread_stack)) {
                printf("Failed to create RAMFS driver thread!\n");
            }

            while (true) {
                uint64_t tag;
                size_t message_size;
                char *message_buffer = (char *)0xa0000000;

                uint64_t message_cookie = anos_recv_message(
                        vfs_channel, &tag, &message_size, message_buffer);

                if (message_cookie) {
#ifdef DEBUG_SYS_IPC
                    printf("SYSTEM::VFS received [0x%016lx] 0x%016lx (%ld "
                           "bytes): %s\n",
                           message_cookie, tag, message_size, message_buffer);
#endif
                    switch (tag) {
                    case VFS_FIND_FS_DRIVER:
                        // find FS driver
                        if (message_size > 5 && message_buffer[0] == 'b' &&
                            message_buffer[1] == 'o' &&
                            message_buffer[2] == 'o' &&
                            message_buffer[3] == 't' &&
                            message_buffer[4] == ':') {
                            anos_reply_message(message_cookie, ramfs_channel);
                        } else {
                            anos_reply_message(message_cookie, 0);
                        }
                        break;
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