/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include <anos.h>
#include <anos/printf.h>
#include <anos/syscalls.h>
#include <anos/types.h>

#include "ramfs.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

extern AnosRAMFSHeader _system_ramfs_start;

int subroutine(int in) { return in * 2; }

volatile int num2;

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

int main(int argc, char **argv) {
    banner();

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
    } else {
        printf("Created new process with PID 0x%016lx\n", new_pid);
    }

    uint64_t vfs_channel = anos_create_channel();

    if (vfs_channel) {
        if (anos_register_channel_name(vfs_channel, "SYSTEM::VFS") == 0) {
            while (true) {
                uint64_t tag;
                size_t message_size;
                char *message_buffer = (void *)0x80000000;
                uint64_t message_cookie = anos_recv_message(
                        vfs_channel, &tag, &message_size, message_buffer);

                if (message_cookie) {
                    printf("SYSTEM::VFS received [0x%016lx] 0x%016lx (%ld "
                           "bytes): %s\n",
                           message_cookie, tag, message_size, message_buffer);

                    AnosRAMFSFileHeader *target = ramfs_find_file(
                            (AnosRAMFSHeader *)&_system_ramfs_start,
                            "test_server.bin");
                    if (target) {
                        anos_reply_message(message_cookie, target->file_length);
                    } else {
                        anos_reply_message(message_cookie, 0);
                    }
                } else {
                    printf("GOT BAD MESSAGE\n");

                    while (1) {
                    }
                }
            }
        }
    }

    printf("Failed to create SYSTEM:: channels!\n");

    while (true) {
        anos_task_sleep_current_secs(5);
    }
}