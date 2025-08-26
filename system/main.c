/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <anos/syscalls.h>
#include <anos/types.h>

#include "path.h"
#include "printf.h"
#include "process.h"
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

#define PROCESS_SPAWN ((1))

#define VM_PAGE_SIZE ((0x1000))
#define DRIVER_THREAD_STACK_PAGES ((0x40))
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

static char __attribute__((aligned(
        VM_PAGE_SIZE))) ramfs_driver_thread_stack[DRIVER_THREAD_STACK_PAGES];
static char __attribute__((aligned(
        VM_PAGE_SIZE))) process_manager_thread_stack[DRIVER_THREAD_STACK_PAGES];

#ifdef TEST_THREAD_KILL
static char __attribute__((
        aligned(VM_PAGE_SIZE))) kamikaze_thread_stack[VM_PAGE_SIZE];
#endif

static uint64_t ramfs_channel;
static uint64_t process_manager_channel;

static inline void banner() {
    printf("\n\nSYSTEM User-mode Supervisor #%s [libanos #%s]\n", VERSION,
           libanos_version());
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

static void handle_file_size_query(const uint64_t message_cookie,
                                   const size_t message_size,
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

        const AnosRAMFSFileHeader *target =
                ramfs_find_file(&_system_ramfs_start, path);

        if (target) {
#ifdef DEBUG_SYS_IPC
            printf("    -> %s found: %ld byte(s)\n", target->file_name,
                   target->file_length);
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

static void handle_file_load_page_query(const uint64_t message_cookie,
                                        const size_t message_size,
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

static void handle_process_spawn_request(const uint64_t message_cookie,
                                         const size_t message_size,
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
        char *str_data = data_ptr;
        size_t str_offset = 0;

        for (uint16_t i = 0; i < argc; i++) {
            if (str_offset >= remaining_size) {
                anos_reply_message(message_cookie, -3);
                return;
            }

            argv[i] = str_data + str_offset;

            while (str_offset < remaining_size &&
                   str_data[str_offset] != '\0') {
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

    const int64_t pid = create_server_process(stack_size, capc,
                                              capc > 0 ? capabilities : NULL,
                                              argc, argc > 0 ? argv : NULL);

    anos_reply_message(message_cookie, pid);
}

static noreturn void process_manager_thread(void) {
    while (true) {
        uint64_t tag;
        size_t message_size;
        char *message_buffer = (char *)0xc0000000;

        const SyscallResult result = anos_recv_message(
                process_manager_channel, &tag, &message_size, message_buffer);

        uint64_t message_cookie = result.value;

        if (result.result == SYSCALL_OK && message_cookie) {
#ifdef DEBUG_SYS_IPC
            printf("SYSTEM::PROCESS received [0x%016lx] 0x%016lx (%ld bytes)\n",
                   message_cookie, tag, message_size);
#endif
            switch (tag) {
            case PROCESS_SPAWN:
                handle_process_spawn_request(message_cookie, message_size,
                                             message_buffer);
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

        const SyscallResult result = anos_recv_message(
                ramfs_channel, &tag, &message_size, message_buffer);

        uint64_t message_cookie = result.value;

        if (result.result == SYSCALL_OK && message_cookie) {
#ifdef DEBUG_SYS_IPC
            printf("SYSTEM::<ramfs> received [0x%016lx] 0x%016lx (%ld "
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

#ifdef TEST_THREAD_KILL
static void kamikaze_thread(void) {
    printf("Kamikaze thread must die!!\n");
    anos_kill_current_task();
}
#endif

int main(int argc, char **argv) {
    banner();

#ifdef TEST_THREAD_KILL
    const SyscallResult create_kill_result = anos_create_thread(
            kamikaze_thread, (uintptr_t)kamikaze_thread_stack);

    if (create_kill_result.result != SYSCALL_OK) {
        printf("Failed to create kamikaze thread...\n");
    }
#endif

    AnosMemInfo meminfo;
    if (anos_get_mem_info(&meminfo).result == SYSCALL_OK) {
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

    const InitCapability new_process_caps[] = {
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT],
                    .capability_id = SYSCALL_ID_DEBUG_PRINT,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR],
                    .capability_id = SYSCALL_ID_DEBUG_CHAR,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_SLEEP],
                    .capability_id = SYSCALL_ID_SLEEP,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_MAP_FIRMWARE_TABLES],
                    .capability_id = SYSCALL_ID_MAP_FIRMWARE_TABLES,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL],
                    .capability_id = SYSCALL_ID_MAP_PHYSICAL,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL],
                    .capability_id = SYSCALL_ID_MAP_VIRTUAL,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_ALLOC_PHYSICAL_PAGES],
                    .capability_id = SYSCALL_ID_ALLOC_PHYSICAL_PAGES,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE],
                    .capability_id = SYSCALL_ID_SEND_MESSAGE,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_FIND_NAMED_CHANNEL],
                    .capability_id = SYSCALL_ID_FIND_NAMED_CHANNEL,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_KILL_CURRENT_TASK],
                    .capability_id = SYSCALL_ID_KILL_CURRENT_TASK,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_ALLOC_INTERRUPT_VECTOR],
                    .capability_id = SYSCALL_ID_ALLOC_INTERRUPT_VECTOR,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_WAIT_INTERRUPT],
                    .capability_id = SYSCALL_ID_WAIT_INTERRUPT,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE],
                    .capability_id = SYSCALL_ID_RECV_MESSAGE,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE],
                    .capability_id = SYSCALL_ID_REPLY_MESSAGE,
            },
            {
                    .capability_cookie =
                            __syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL],
                    .capability_id = SYSCALL_ID_CREATE_CHANNEL,
            },
            {
                    .capability_cookie = __syscall_capabilities
                            [SYSCALL_ID_REGISTER_NAMED_CHANNEL],
                    .capability_id = SYSCALL_ID_REGISTER_NAMED_CHANNEL,
            },
    };

    const SyscallResult create_vfs_result = anos_create_channel();
    const uint64_t vfs_channel = create_vfs_result.value;

    const SyscallResult create_ramfs_result = anos_create_channel();
    ramfs_channel = create_ramfs_result.value;

    const SyscallResult create_process_manager_result = anos_create_channel();
    process_manager_channel = create_process_manager_result.value;

    bool all_created_ok = create_vfs_result.result == SYSCALL_OK &&
                          create_ramfs_result.result == SYSCALL_OK &&
                          create_process_manager_result.result == SYSCALL_OK;

    if (all_created_ok && vfs_channel && ramfs_channel &&
        process_manager_channel) {
        if (anos_register_channel_name(vfs_channel, "SYSTEM::VFS").result ==
            SYSCALL_OK) {
            if (anos_register_channel_name(process_manager_channel,
                                           "SYSTEM::PROCESS")
                        .result != SYSCALL_OK) {
                printf("Failed to register SYSTEM::PROCESS channel!\n");
            }
            // set up RAMFS driver thread
            SyscallResult create_thread_result = anos_create_thread(
                    ramfs_driver_thread, (uintptr_t)ramfs_driver_thread_stack);

            if (create_thread_result.result != SYSCALL_OK) {
                printf("Failed to create RAMFS driver thread!\n");
            }

            create_thread_result =
                    anos_create_thread(process_manager_thread,
                                       (uintptr_t)process_manager_thread_stack);

            // set up process manager thread
            if (create_thread_result.result != SYSCALL_OK) {
                printf("Failed to create process manager thread!\n");
            }

#ifdef TEST_BEEP_BOOP
            const char *test_server_argv[] = {"boot:/test_server.elf",
                                              "Hello, world!"};

            const int64_t test_server_pid = create_server_process(
                    0x100000, 3, new_process_caps, 2, test_server_argv);
            if (test_server_pid < 0) {
                printf("%s: Failed to create server process\n",
                       "boot:/test_server.elf");
            }
#endif

            const char *devman_argv[] = {"boot:/devman.elf"};

            const int64_t devman_pid = create_server_process(
                    0x100000, 16, new_process_caps, 1, devman_argv);
            if (devman_pid < 0) {
                printf("%s: Failed to create server process\n",
                       "boot:/devman.elf");
            }

            while (true) {
                uint64_t tag;
                size_t message_size;
                char *message_buffer = (char *)0xa0000000;

                const SyscallResult result = anos_recv_message(
                        vfs_channel, &tag, &message_size, message_buffer);

                uint64_t message_cookie = result.value;

                if (result.result == SYSCALL_OK && message_cookie) {
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