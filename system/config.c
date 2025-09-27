/*
 * Configuration handling for SYSTEM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <string.h>

#include <anos/syscalls.h>

#include "config.h"
#include "jansson.h"
#include "loader.h"
#include "process.h"

#ifdef DEBUG_LOAD_CONFIG
#include <stdio.h>
#define load_debug(...) printf("LoadConfig: " __VA_ARGS__)
#else
#define load_debug(...)
#endif

#ifdef DEBUG_PROCESS_CONFIG
#include <stdio.h>
#define process_debug(...) printf("ProcessConfig: " __VA_ARGS__)
#else
#define process_debug(...)
#endif

static const char *const NAME_KEY = "name";
static const char *const PATH_KEY = "path";
static const char *const STACK_SIZE_KEY = "stack_size";
static const char *const CAPS_KEY = "capabilities";
static const char *const BOOT_SERVERS_KEY = "boot_servers";
static const char *const ARGS_KEY = "arguments";

static const char *const SYSCALL_IDENTIFIERS[] = {"SYSCALL_DEBUG_PRINT",
                                                  "SYSCALL_DEBUG_CHAR",
                                                  "SYSCALL_CREATE_THREAD",
                                                  "SYSCALL_MEMSTATS",
                                                  "SYSCALL_SLEEP",
                                                  "SYSCALL_CREATE_PROCESS",
                                                  "SYSCALL_MAP_VIRTUAL",
                                                  "SYSCALL_SEND_MESSAGE",
                                                  "SYSCALL_RECV_MESSAGE",
                                                  "SYSCALL_REPLY_MESSAGE",
                                                  "SYSCALL_CREATE_CHANNEL",
                                                  "SYSCALL_DESTROY_CHANNEL",
                                                  "SYSCALL_REGISTER_NAMED_CHANNEL",
                                                  "SYSCALL_DEREGISTER_NAMED_CHANNEL",
                                                  "SYSCALL_FIND_NAMED_CHANNEL",
                                                  "SYSCALL_KILL_CURRENT_TASK",
                                                  "SYSCALL_UNMAP_VIRTUAL",
                                                  "SYSCALL_CREATE_REGION",
                                                  "SYSCALL_DESTROY_REGION",
                                                  "SYSCALL_MAP_FIRMWARE_TABLES",
                                                  "SYSCALL_MAP_PHYSICAL",
                                                  "SYSCALL_ALLOC_PHYSICAL_PAGES",
                                                  "SYSCALL_ALLOC_INTERRUPT_VECTOR",
                                                  "SYSCALL_WAIT_INTERRUPT",
                                                  "SYSCALL_READ_KERNEL_LOG",
                                                  "SYSCALL_GET_FRAMEBUFFER_PHYS"};

static constexpr int SYSCALL_IDENTIFIER_COUNT = sizeof(SYSCALL_IDENTIFIERS) / sizeof(SYSCALL_IDENTIFIERS[0]);

static constexpr uintptr_t MSG_BUFFER_ADDR_V = 0x5fff000;

extern uint64_t __syscall_capabilities[];

const char *load_config_file(const char *filename) {
    char *load_buffer = nullptr;
    SyscallResult result = anos_find_named_channel("SYSTEM::VFS");
    const uint64_t sys_vfs_cookie = result.value;

    if (result.result != SYSCALL_OK || !sys_vfs_cookie) {
        load_debug("Failed to find named VFS channel\n");
        return nullptr;
    }

    const SyscallResultP map_result = anos_map_virtual(MAX_IPC_BUFFER_SIZE, MSG_BUFFER_ADDR_V,
                                                       ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    char *msg_buffer = map_result.value;

    if (result.result != SYSCALL_OK || msg_buffer == nullptr) {
        load_debug("Failed to map message buffer\n");
        return nullptr;
    }

    strncpy(msg_buffer, filename, MAX_IPC_BUFFER_SIZE);

    // TODO this should be strnlen, unbounded strlen isn't safe here!
    const size_t filename_size = strlen(filename) + 1;
    result = anos_send_message(sys_vfs_cookie, 1, filename_size, msg_buffer);
    const uint64_t sys_ramfs_cookie = result.value;

    if (result.result != SYSCALL_OK || !sys_ramfs_cookie) {
        load_debug("Failed to find filesystem driver\n");
        goto error;
    }

    result = anos_send_message(sys_ramfs_cookie, SYS_VFS_TAG_GET_SIZE, filename_size, msg_buffer);

    const size_t file_size = result.value;

    if (result.result == SYSCALL_OK && file_size) {
        load_debug("Found config file %s\n", filename);

        load_buffer = malloc(file_size + 1);

        if (!load_buffer) {
            load_debug("Failed to allocate memory for config file\n");
            goto error;
        }

        size_t bytes_remain = file_size;
        size_t file_offset = 0;
        size_t load_buffer_ptr = 0;

        while (true) {
            uint64_t *const pos = (uint64_t *)msg_buffer;
            *pos = file_offset;

            strcpy(msg_buffer + sizeof(uint64_t), filename);

            result = anos_send_message(sys_ramfs_cookie, SYS_VFS_TAG_LOAD_PAGE, filename_size + 9, msg_buffer);

            const uint64_t loaded_bytes = result.value;

            if (result.result != SYSCALL_OK || !loaded_bytes) {
                printf("FAILED TO LOAD: 0 bytes (result: %ld)\n", (long)result.result);

                free(load_buffer);
                anos_unmap_virtual(MAX_IPC_BUFFER_SIZE, MSG_BUFFER_ADDR_V);
                return nullptr;
            }

            for (int i = 0; i < loaded_bytes; i++) {
                load_buffer[i + load_buffer_ptr] = msg_buffer[i];
            }

            bytes_remain -= loaded_bytes;
            file_offset += loaded_bytes;
            load_buffer_ptr += loaded_bytes;

            if (!bytes_remain) {
                break;
            }
        }

        anos_unmap_virtual(MAX_IPC_BUFFER_SIZE, MSG_BUFFER_ADDR_V);
        load_buffer[load_buffer_ptr] = '\0';
        return load_buffer;

    } else {
        printf("WARN: SYSTEM config file not found\n");
    }

error:
    if (load_buffer) {
        free((void *)load_buffer);
    }
    anos_unmap_virtual(MAX_IPC_BUFFER_SIZE, MSG_BUFFER_ADDR_V);
    return nullptr;
}

static uint64_t cap_str_to_id(const char *str) {
    for (int i = 0; i < SYSCALL_IDENTIFIER_COUNT; i++) {
        if (strncmp(SYSCALL_IDENTIFIERS[i], str, strlen(SYSCALL_IDENTIFIERS[i])) == 0) {
            return i + 1;
        }
    }

    return 0;
}

static InitCapability *build_process_caps(const json_t *config_caps_array, const size_t caps_array_size) {
    InitCapability *process_caps_array = nullptr;

    if (json_is_array(config_caps_array)) {
        if (caps_array_size > 0) {
            process_caps_array = malloc(sizeof(InitCapability) * caps_array_size);

            if (!process_caps_array) {
                process_debug("    Failed to allocate %lu bytes of memory\n", sizeof(InitCapability) * caps_array_size);
                goto error;
            }

            process_debug("    Capabilities:\n");

            for (int i = 0; i < caps_array_size; i++) {
                process_caps_array[i].capability_id = 0;
                process_caps_array[i].capability_cookie = 0;

                const json_t *cap = json_array_get(config_caps_array, i);

                if (!json_is_string(cap)) {
                    process_debug("Server capabilities [entry %d] contains non-string\n", i);
                    goto error;
                }

                const char *cap_str = json_string_value(cap);
                const uint64_t cap_id = cap_str_to_id(cap_str);

                if (cap_id == 0 || cap_id >= SYSCALL_ID_END) {
                    process_debug("Server capabilities [entry %d] contains invalid capability: '%s'\n", i, cap_str);
                    goto error;
                }

                process_debug("        - %s (%lu)\n", cap_str, cap_id);

                process_caps_array[i].capability_id = cap_id;
                process_caps_array[i].capability_cookie = __syscall_capabilities[cap_id];
            }

            return process_caps_array;
        }

        process_debug("Server capabilities array present but empty\n");

    } else {
        process_debug("Server capabilities array not present or invalid\n");
    }

error:
    if (process_caps_array != nullptr) {
        free(process_caps_array);
    }

    return nullptr;
}

/*
 * N.B. This returns an array that reuses the character data owned by the JSON object,
 * so we mustn't decref that until it's been copied (by a create process call).
 */
static const char **build_process_args(const json_t *args, const size_t args_array_size, const char *path) {

    // We're always going to populate at least one entry, for the path, so just
    // create this immediately and take care of that...
    const char **process_args_array =
            malloc(sizeof(char *) * (args_array_size + 1)); // plus one to leave space for path!

    if (!process_args_array) {
        process_debug("    Failed to allocate %lu bytes of memory\n", sizeof(char *) * (args_array_size + 1));
        goto error;
    }

    process_args_array[0] = path;

    if (json_is_array(args)) {
        if (args_array_size > 0) {
            process_debug("    Arguments:\n");

            for (int i = 0; i < args_array_size; i++) {
                const json_t *arg = json_array_get(args, i);

                if (!json_is_string(arg)) {
                    process_debug("Server arguments [entry %d] contains non-string\n", i);
                    goto error;
                }

                process_args_array[i + 1] = json_string_value(arg);

                process_debug("        - %s\n", process_args_array[i + 1]);
            }
        }
    } else {
        process_debug("Server arguments array not present or invalid\n");
    }

    return process_args_array;

error:
    if (process_args_array != nullptr) {
        free(process_args_array);
    }
    return nullptr;
}

static ProcessConfigResult process_boot_servers(const json_t *boot_servers) {
    for (int i = 0; i < json_array_size(boot_servers); i++) {
        const json_t *server = json_array_get(boot_servers, i);

        if (!json_is_object(server)) {
            process_debug("Boot server array contains non-object");
            return PROCESS_CONFIG_INVALID;
        }

        const json_t *name = json_object_get(server, NAME_KEY);
        const json_t *path = json_object_get(server, PATH_KEY);
        const json_t *stack_size = json_object_get(server, STACK_SIZE_KEY);

        if (!json_is_string(name)) {
            process_debug("Server name [entry %d] is not a string\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        if (!json_is_string(path)) {
            process_debug("Server path [entry %d] is not a string\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        if (!json_is_integer(stack_size)) {
            process_debug("Server stack size [entry %d] is not an integer\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        const char *name_str = json_string_value(name);
        const char *path_str = json_string_value(path);
        const uint64_t stack_size_int = json_integer_value(stack_size);

        process_debug("Server %d:\n", i);
        process_debug("    Name: %s\n", name_str);
        process_debug("    Path: %s\n", path_str);
        process_debug("    Stack size: %lu\n", stack_size_int);

        const json_t *caps = json_object_get(server, CAPS_KEY);
        const size_t caps_array_size = json_array_size(caps);
        const InitCapability *process_caps = build_process_caps(caps, caps_array_size);

        if (caps_array_size > 0 && !process_caps) {
            process_debug("Failed to process capabilities for server [entry %d]\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        const json_t *args = json_object_get(server, ARGS_KEY);
        const size_t args_array_size = json_array_size(args);
        const char **process_args = build_process_args(args, args_array_size, path_str);

        if (!process_args) {
            process_debug("Failed to process arguments for server [entry %d]\n", i);
            if (process_caps) {
                free((void *)process_caps);
            }
            return PROCESS_CONFIG_INVALID;
        }

        process_debug("\n");

        const int64_t pid =
                create_server_process(stack_size_int, caps_array_size, process_caps, args_array_size + 1, process_args);

        // Free allocated memory after process creation
        if (process_caps) {
            free((void *)process_caps);
        }
        if (process_args) {
            free(process_args);
        }

        if (pid < 0) {
            process_debug("Warning: Failed to start boot process: %s (%s)\n", name_str, path_str);
            return PROCESS_CONFIG_FAILURE;
        }

        printf("Started %s with PID %ld\n", name_str, pid);
    }

    return PROCESS_CONFIG_OK;
}

ProcessConfigResult process_config(const char *json) {
    ProcessConfigResult result = PROCESS_CONFIG_OK;

    json_error_t json_error;
    json_t *root = json_loads(json, JSON_DECODE_ANY, &json_error);

    if (!root) {
        process_debug("Error on line %d: %s\n", json_error.line, json_error.text);
        result = PROCESS_CONFIG_INVALID;
        goto cleanup;
    }

    if (!json_is_object(root)) {
        process_debug("Root is not object\n");
        result = PROCESS_CONFIG_INVALID;
        goto cleanup;
    }

    const json_t *boot_servers = json_object_get(root, BOOT_SERVERS_KEY);

    if (json_is_array(boot_servers)) {
        result = process_boot_servers(boot_servers);
    } else {
        if (!boot_servers) {
            process_debug("boot_servers key not found\n");
        } else {
            process_debug("boot_servers key does not reference an array\n");
        }
    }

cleanup:
    json_decref(root);
    return result;
}