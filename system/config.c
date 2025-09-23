/*
 * Configuration handling for SYSTEM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <string.h>

#include "config.h"
#include "jansson.h"

#ifdef DEBUG_PROCESS_CONFIG
#include <stdio.h>
#define process_debug(...) printf("ProcessConfig: " __VA_ARGS__)
#else
#define process_debug(...)
#endif

static const char *const NAME_KEY = "name";
static const char *const PATH_KEY = "path";
static const char *const CAPS_KEY = "capabilities";
static const char *const BOOT_SERVERS_KEY = "boot_servers";
static const char *const ARGS_KEY = "arguments";

static const char *const SYSCALL_IDENTIFIERS[] = {
        "SYSCALL_DEBUG_PRINT",
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

static constexpr int SYSCALL_IDENTIFIER_COUNT =
        sizeof(SYSCALL_IDENTIFIERS) / sizeof(SYSCALL_IDENTIFIERS[0]);

static uint64_t cap_str_to_id(const char *str) {
    for (int i = 0; i < SYSCALL_IDENTIFIER_COUNT; i++) {
        if (strncmp(SYSCALL_IDENTIFIERS[i], str,
                    strlen(SYSCALL_IDENTIFIERS[i])) == 0) {
            return i + 1;
        }
    }

    return 0;
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

        if (!json_is_string(name) || !json_is_string(path)) {
            process_debug("Boot server name or path [entry %d] is invalid\n",
                          i);
            return PROCESS_CONFIG_INVALID;
        }

        if (!json_is_string(name)) {
            process_debug("Server name [entry %d] is not a string\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        if (!json_is_string(path)) {
            process_debug("Server path [entry %d] is not a string\n", i);
            return PROCESS_CONFIG_INVALID;
        }

        printf("Server %d:\n", i);
        printf("    Name: %s\n", json_string_value(name));
        printf("    Path: %s\n", json_string_value(path));

        const json_t *caps = json_object_get(server, CAPS_KEY);
        if (json_is_array(caps)) {
            if (json_array_size(caps) > 0) {
                printf("    Capabilities:\n");
            }

            for (int j = 0; j < json_array_size(caps); j++) {
                const json_t *cap = json_array_get(caps, j);

                if (!json_is_string(cap)) {
                    process_debug("Server array [entry %d] capabilities [entry "
                                  "%d] contains non-string\n",
                                  i, j);
                    return PROCESS_CONFIG_INVALID;
                }

                const char *cap_str = json_string_value(cap);
                const uint64_t cap_id = cap_str_to_id(cap_str);

                if (cap_id == 0) {
                    process_debug("Server array [entry %d] capabilities [entry "
                                  "%d] contains invalid capability: '%s'\n",
                                  i, j, cap_str);
                    return PROCESS_CONFIG_INVALID;
                }

                printf("        - %s (%lu)\n", cap_str, cap_id);
            }
        } else {
            process_debug(
                    "Server [%d] capabilities array not present or invalid\n",
                    i);
        }

        const json_t *args = json_object_get(server, ARGS_KEY);
        if (json_is_array(args)) {
            if (json_array_size(args) > 0) {
                printf("    Arguments:\n");
            }

            for (int j = 0; j < json_array_size(args); j++) {
                const json_t *arg = json_array_get(args, j);

                if (!json_is_string(arg)) {
                    process_debug("Server array [entry %d] arguments [entry "
                                  "%d] contains non-string\n",
                                  i, j);
                    return PROCESS_CONFIG_INVALID;
                }

                const char *arg_str = json_string_value(arg);

                printf("        - %s\n", arg_str);
            }
        } else {
            process_debug(
                    "Server [%d] arguments array not present or invalid\n", i);
        }

        printf("\n");
    }

    return PROCESS_CONFIG_OK;
}

ProcessConfigResult process_config(const char *json) {
    ProcessConfigResult result = PROCESS_CONFIG_OK;

    json_error_t json_error;
    json_t *root = json_loads(json, JSON_DECODE_ANY, &json_error);

    if (!root) {
        process_debug("Error on line %d: %s\n", json_error.line,
                      json_error.text);
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