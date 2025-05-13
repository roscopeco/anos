/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>
#include <string.h>

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

typedef struct {
    uint64_t capability_id;
    uint64_t capability_cookie;
} InitCapability;

typedef struct {
    uint64_t value_count;
    uintptr_t *data;
    size_t allocated_size;
} InitStackValues;

typedef struct {
    uint64_t start_byte_ofs;
    char name[];
} FileLoadPageQuery;

extern void *_code_start;
extern void *_code_end;
extern void *_bss_start;
extern void *_bss_end;
extern void *__user_stack_top;

extern AnosRAMFSHeader _system_ramfs_start;

extern uint64_t __syscall_capabilities[];

static char __attribute__((
        aligned(VM_PAGE_SIZE))) ramfs_driver_thread_stack[VM_PAGE_SIZE];

int strnlen(const char *param, int maxlen);

static char __attribute__((aligned(VM_PAGE_SIZE))) argv_p[4096];
static char __attribute__((aligned(VM_PAGE_SIZE))) newstack_p[4096];

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

static inline unsigned int round_up_to_page_size(const size_t size) {
    return (size + VM_PAGE_SIZE - 1) & ~(VM_PAGE_SIZE - 1);
}

static inline unsigned int round_up_to_machine_word_size(const size_t size) {
    return (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
}

static bool build_new_process_init_values(const uintptr_t stack_top_addr,
                                          const uint16_t cap_count,
                                          const InitCapability *capabilities,
                                          const uint16_t argc,
                                          const char *argv[],
                                          InitStackValues *out_init_values) {
    if (!out_init_values) {
        return false;
    }

    if (argc > MAX_ARG_COUNT) {
        return false;
    }

    // TODO this is a bit inefficient, running through args twice...
    uint64_t total_argv_len = 0;
    if (argc > 0 && argv) {
        for (int i = 0; i < argc; i++) {
            if (argv[i]) {
                total_argv_len += strnlen(argv[i], MAX_ARG_LENGTH - 1) + 1;
            }
        }
    }

    // round up to machine words and convert to value count
    const uint64_t total_argv_words =
            round_up_to_machine_word_size(total_argv_len) / sizeof(uintptr_t);

    const uint64_t value_count = total_argv_words + (cap_count * 2) + argc +
                                 INIT_STACK_STATIC_VALUE_COUNT;

    if (value_count > MAX_STACK_VALUE_COUNT) {
        return false;
    }

    // MAX_ARGC is 512, so we only need a page for this...
    uintptr_t *argv_pointers = (uintptr_t *)argv_p;

    if (!argv_pointers) {
        return false;
    }

    const uint64_t stack_blocks_needed = value_count / 512;
    uintptr_t *new_stack = (uintptr_t *)newstack_p;

    if (stack_blocks_needed > 1) {
        // TODO remove this once we have malloc'd blocks!
        return false;
    }

    if (!new_stack) {
        // todo free argv pointer page
        return false;
    }

    const uintptr_t stack_bottom_addr =
            stack_top_addr - (sizeof(uintptr_t) * value_count);

    // copy string data first...
    uint8_t *str_data_ptr =
            (uint8_t *)new_stack + value_count * sizeof(uintptr_t);

    for (int i = argc - 1; i >= 0; i--) {
        if (argv[i]) {
            const int len = strnlen(argv[i], MAX_ARG_LENGTH - 1);

            *--str_data_ptr =
                    '\0'; // ensure null term because strncpy doesn't...
            str_data_ptr -= len;
            strncpy((char *)str_data_ptr, argv[i], len);

            argv_pointers[i] = stack_bottom_addr +
                               ((uintptr_t)str_data_ptr - (uintptr_t)new_stack);
        } else {
            str_data_ptr -= 1;
            *str_data_ptr = 0;
        }
    }

    const uint8_t string_padding = (uintptr_t)str_data_ptr % sizeof(uintptr_t);
    for (int i = 0; i < string_padding; i++) {
        *--str_data_ptr = 0;
    }

    // copy capabilities
    uintptr_t *long_data_pointer = (uintptr_t *)str_data_ptr;

    if (cap_count > 0 && capabilities) {
        for (int i = cap_count - 1; i >= 0; i--) {
            *--long_data_pointer = capabilities[i].capability_cookie;
            *--long_data_pointer = capabilities[i].capability_id;
        }
    }

    const uintptr_t cap_ptr =
            (cap_count > 0 && capabilities)
                    ? stack_bottom_addr + ((uintptr_t)long_data_pointer -
                                           (uintptr_t)new_stack)
                    : 0;

    // copy argv array
    for (int i = argc - 1; i >= 0; i--) {
        *--long_data_pointer = argv_pointers[i];
    }

    const uintptr_t argv_ptr =
            stack_bottom_addr +
            ((uintptr_t)long_data_pointer - (uintptr_t)new_stack);

    // set up argv / argc
    *--long_data_pointer = argv_ptr;
    *--long_data_pointer = argc;

    // set up capv / capc
    *--long_data_pointer = cap_ptr;
    *--long_data_pointer = cap_count;

    out_init_values->value_count = value_count;
    out_init_values->data = new_stack;
    out_init_values->allocated_size = stack_blocks_needed;

    return true;
}

/*
 * When calling this, the executable name must be argv[0]!
 */
int64_t create_server_process(const uint64_t stack_size, const uint16_t capc,
                              const InitCapability *capv, const uint16_t argc,
                              const char *argv[]) {
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

    InitStackValues init_stack_values;

    if (!build_new_process_init_values(0x80000000, capc, capv, argc, argv,
                                       &init_stack_values)) {
        // failed to init stack...
        return 0;
    }

    ProcessCreateParams process_create_params;

    process_create_params.entry_point = initial_server_loader;
    process_create_params.stack_base = STACK_TOP - stack_size;
    process_create_params.stack_size = stack_size;
    process_create_params.region_count = 2;
    process_create_params.regions = regions;
    process_create_params.stack_value_count = init_stack_values.value_count;
    process_create_params.stack_values = init_stack_values.data;

    return anos_create_process(&process_create_params);

    // TODO free stack_values
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
    };

    const char *new_process_argv[] = {"boot:/test_server.elf", "Hello, world!"};

    const int64_t new_pid = create_server_process(0x100000, 3, new_process_caps,
                                                  2, new_process_argv);
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