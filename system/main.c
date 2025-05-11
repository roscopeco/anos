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

#include "loader.h"
#include "path.h"
#include "printf.h"
#include "ramfs.h"

#if (__STDC_VERSION__ < 202000)
// TODO Apple clang doesn't support constexpr yet - Jan 2025
#ifndef constexpr
#define constexpr const
#endif
#endif

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

#define VM_PAGE_SIZE ((0x1000))

extern void *_code_start;
extern void *_code_end;
extern void *_bss_start;
extern void *_bss_end;
extern void *__user_stack_top;

extern AnosRAMFSHeader _system_ramfs_start;

static char __attribute__((
        aligned(VM_PAGE_SIZE))) ramfs_driver_thread_stack[VM_PAGE_SIZE];

#ifdef TEST_THREAD_KILL
static char __attribute__((
        aligned(VM_PAGE_SIZE))) kamikaze_thread_stack[VM_PAGE_SIZE];
#endif

static uint64_t ramfs_channel;

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

static noreturn void ramfs_driver_thread(void) {
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

#ifdef TEST_THREAD_KILL
static void kamikaze_thread(void) {
    printf("Kamikaze thread must die!!\n");
    anos_kill_current_task();
}
#endif

static inline unsigned int round_up_to_page_size(size_t size) {
    return (size + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
}

int64_t create_server_process(const char *file_name,
                              const uint64_t stack_size) {
    // We need to map in SYSTEM's code and BSS segments temporarily,
    // so that the initial_server_loader (loader.c) can do its thing
    // in the new process - it needs our capabilities etc to be
    // able to actually load the binary.
    //
    // The code there is responsible for removing these mappings
    // before handing off control to the new process.
    //
    // NOTE keep this in-step with unmapping in loader.c!
    ProcessMemoryRegion regions[2] = {
            {
                    .start = (uintptr_t)&_code_start,
                    .len_bytes = round_up_to_page_size((uintptr_t)&_code_end -
                                                       (uintptr_t)&_code_start),
            },
            {
                    .start = (uintptr_t)&_bss_start,
                    .len_bytes = round_up_to_page_size((uintptr_t)&_bss_end -
                                                       (uintptr_t)&_bss_start),
            },
    };

    extern uint64_t __syscall_capabilities[];

    constexpr uint8_t init_stack_cap_count = 3;
    const uintptr_t init_stack_cap_ptr =
            ((uintptr_t)(&__user_stack_top)) -
            (init_stack_cap_count * INIT_STACK_CAP_SIZE_LONGS *
             sizeof(uintptr_t));

    constexpr uint8_t init_stack_argc = 1;
    const uintptr_t init_stack_argv_ptr =
            init_stack_cap_ptr - (init_stack_argc * sizeof(uintptr_t));

    constexpr uint16_t init_stack_value_count =
            init_stack_cap_count * INIT_STACK_CAP_SIZE_LONGS + init_stack_argc +
            INIT_STACK_STATIC_VALUE_COUNT;

    uint64_t stack_values[init_stack_value_count] = {
            // Initial capabilities
            __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT],
            SYSCALL_ID_DEBUG_PRINT,
            __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR],
            SYSCALL_ID_DEBUG_CHAR,
            __syscall_capabilities[SYSCALL_ID_SLEEP],
            SYSCALL_ID_SLEEP,

            // argc
            // TODO stack the rest of argv here...
            (uint64_t)file_name,

            // static pointers / counts
            init_stack_argv_ptr,
            init_stack_argc,
            init_stack_cap_ptr,
            init_stack_cap_count,
    };

    ProcessCreateParams process_create_params;

    process_create_params.entry_point = initial_server_loader;
    process_create_params.stack_base = STACK_TOP - stack_size;
    process_create_params.stack_size = stack_size;
    process_create_params.region_count = 2;
    process_create_params.regions = regions;
    process_create_params.stack_value_count = init_stack_value_count;
    process_create_params.stack_values = stack_values;

    return anos_create_process(&process_create_params);
}

int main(int argc, char **argv) {
    banner();

#ifdef TEST_THREAD_KILL
    if (!anos_create_thread(kamikaze_thread,
                            (uintptr_t)kamikaze_thread_stack)) {
        printf("Failed to create kamikaze thread...\n");
    }
#endif

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

    const int64_t new_pid =
            create_server_process("boot:/test_server.elf", 0x100000);
    if (new_pid < 0) {
        printf("%s: Failed to create server process\n",
               "boot:/test_server.elf");
    }

    const uint64_t vfs_channel = anos_create_channel();
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