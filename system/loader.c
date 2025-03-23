#include <stdnoreturn.h>

#include <anos.h>
#include <anos/printf.h>
#include <anos/syscalls.h>

#ifndef NULL
#define NULL (((void *)0))
#endif

#define SYS_VFS_TAG_GET_SIZE ((0x1))

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
}

noreturn int initial_server_loader(void) {
    anos_kprint("Starting the other thing...\n");

    uint64_t sys_vfs_cookie = anos_find_named_channel("SYSTEM::VFS");

    if (!sys_vfs_cookie) {
        anos_kprint("Failed to find named channel\n");
        sleep_loop();
    }

    char *msg_buffer = anos_map_virtual(0x1000, 0x2000000);

    strcpy_hack(msg_buffer, "boot:/test_server.bin");

    uint64_t exec_size = anos_send_message(sys_vfs_cookie, SYS_VFS_TAG_GET_SIZE,
                                           22, msg_buffer);

    printf("test_server.bin is %ld byte(s)\n", exec_size);

    printf("Message buffer now: %s\n", msg_buffer);
    // if (exec_size) {
    //     void *buffer = anos_map_virtual(exec_size, 0x2000000);

    //     if (buffer) {
    //         int result = anos_send_message("/system/obj_load", "/vfs/boot:/test_server.bin", buffer);

    //         if (result) {
    //             ServerEntrypoint sep = (ServerEntrypoint)0x2000000;
    //             sep();
    //         } else {
    //             anos_kprint("Unable to load executable\n");
    //         }
    //     } else {
    //         anos_kprint("Unable to map memory\n");
    //     }
    // } else {
    //     anos_kprint("Unable to get size\n");
    // }

    anos_kprint("Initial server exec failed. Dying.\n");

    sleep_loop();

    __builtin_unreachable();
}