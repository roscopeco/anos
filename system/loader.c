#include <stdnoreturn.h>

#include <anos.h>
#include <anos/syscalls.h>

#ifndef NULL
#define NULL (((void *)0))
#endif

typedef void (*ServerEntrypoint)(void);

noreturn int initial_server_loader(void) {
    anos_kprint("Starting the other thing...\n");

    // uint64_t exec_size = anos_send_message("/system/obj_get_size", "/vfs/boot:/test_server.bin", NULL);

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

    while (1) {
        anos_task_sleep_current_secs(1000);
    }

    __builtin_unreachable();
}