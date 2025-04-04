/*
 * Bootstrap process loader
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This serves as the entrypoint for a new process loaded
 * from the RAMFS by SYSTEM.
 */

#include <stdbool.h>
#include <stdnoreturn.h>

#include <anos.h>
#include <anos/printf.h>
#include <anos/syscalls.h>

#ifndef NULL
#define NULL (((void *)0))
#endif

#define SYS_VFS_TAG_GET_SIZE ((0x1))
#define SYS_VFS_TAG_LOAD_PAGE ((0x2))

typedef void (*ServerEntrypoint)(void);

static noreturn void sleep_loop(void) {
    while (1) {
        anos_task_sleep_current_secs(1000);
    }
}

static void strcpy_hack(char *dst, char *src) {
    while (*src) {
        *dst++ = *src++;
    }
    *dst = 0;
}

noreturn int initial_server_loader(void) {
    anos_kprint("\nLoading 'boot:/test_server.bin'...\n");

    uint64_t sys_vfs_cookie = anos_find_named_channel("SYSTEM::VFS");

    if (!sys_vfs_cookie) {
        anos_kprint("Failed to find named VFS channel\n");
        sleep_loop();
    }

    char *msg_buffer = anos_map_virtual(0x1000, 0x1fff000);

    strcpy_hack(msg_buffer, "boot:/test_server.bin");

    uint64_t sys_ramfs_cookie =
            anos_send_message(sys_vfs_cookie, 1, 22, msg_buffer);

    if (!sys_ramfs_cookie) {
        anos_kprint("FAILED TO FIND RAMFS DRIVER\n");
        while (true) {
            sleep_loop();
        }
    }

    uint64_t exec_size = anos_send_message(
            sys_ramfs_cookie, SYS_VFS_TAG_GET_SIZE, 22, msg_buffer);

    if (exec_size) {
        bool success = true;
        char *buffer = anos_map_virtual(exec_size, 0x2000000);

        if (buffer) {
            for (int i = 0; i < exec_size; i += 0x1000) {
                uint64_t *pos = (uint64_t *)msg_buffer;
                *pos = i;
                strcpy_hack(msg_buffer + sizeof(uint64_t),
                            "boot:/test_server.bin");

                int loaded_bytes = anos_send_message(sys_ramfs_cookie,
                                                     SYS_VFS_TAG_LOAD_PAGE, 26,
                                                     msg_buffer);

                if (loaded_bytes) {
                    for (int j = 0; j < loaded_bytes; j++) {
                        buffer[i + j] = msg_buffer[j];
                    }
                } else {
                    anos_kprint("FAILED TO LOAD: 0 bytes\n");
                    success = false;
                    break;
                }
            }

            if (success) {
                ServerEntrypoint sep = (ServerEntrypoint)0x2000000;
                sep();
            } else {
                anos_kprint("Unable to load executable\n");
            }
        } else {
            anos_kprint("Unable to map memory\n");
        }
    } else {
        anos_kprint("Unable to get size\n");
    }

    anos_kprint("Initial server exec failed. Dying.\n");

    sleep_loop();

    __builtin_unreachable();
}