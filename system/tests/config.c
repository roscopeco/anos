/* Unit tests for the config subsystem.
 *
 * Copyright (c) 2025 Ross Bamford. See LICENSE for details.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "munit.h"

/* Mock jansson library functions to avoid dependency */

typedef struct json_t {
    int type;
    union {
        const char *string_value;
        int64_t integer_value;
        struct json_t **array_items;
        struct {
            const char **keys;
            struct json_t **values;
        } object;
    } data;
    size_t size;
} json_t;

typedef struct {
    int line;
    char text[256];
} json_error_t;

#define JSON_DECODE_ANY 0

/* JSON type constants */
#define JSON_OBJECT 1
#define JSON_ARRAY 2
#define JSON_STRING 3
#define JSON_INTEGER 4

static json_t *mock_json_root = NULL;
static int mock_json_loads_should_fail = 0;

/* Mock jansson functions */
json_t *json_loads(const char *input, size_t flags, json_error_t *error) {
    (void)input;
    (void)flags;

    if (mock_json_loads_should_fail) {
        if (error) {
            error->line = 1;
            strcpy(error->text, "Mock JSON parse error");
        }
        return NULL;
    }

    return mock_json_root;
}

int json_is_object(const json_t *json) { return json && json->type == JSON_OBJECT; }

int json_is_array(const json_t *json) { return json && json->type == JSON_ARRAY; }

int json_is_string(const json_t *json) { return json && json->type == JSON_STRING; }

int json_is_integer(const json_t *json) { return json && json->type == JSON_INTEGER; }

json_t *json_object_get(const json_t *object, const char *key) {
    if (!object || object->type != JSON_OBJECT)
        return NULL;

    for (size_t i = 0; i < object->size; i++) {
        if (strcmp(object->data.object.keys[i], key) == 0) {
            return object->data.object.values[i];
        }
    }
    return NULL;
}

json_t *json_array_get(const json_t *array, size_t index) {
    if (!array || array->type != JSON_ARRAY || index >= array->size) {
        return NULL;
    }
    return array->data.array_items[index];
}

size_t json_array_size(const json_t *array) {
    if (!array || array->type != JSON_ARRAY)
        return 0;
    return array->size;
}

const char *json_string_value(const json_t *string) {
    if (!string || string->type != JSON_STRING)
        return NULL;
    return string->data.string_value;
}

int64_t json_integer_value(const json_t *integer) {
    if (!integer || integer->type != JSON_INTEGER)
        return 0;
    return integer->data.integer_value;
}

void json_decref(json_t *json) {
    /* Mock - we'll manage memory manually in tests */
    (void)json;
}

/* Mock syscall functions */
static int mock_find_named_channel_should_fail = 0;
static int mock_map_virtual_should_fail = 0;
static int mock_send_message_should_fail = 0;
static void *mock_mapped_memory = NULL;

typedef struct {
    int64_t result;
    uint64_t value;
} SyscallResult;

typedef struct {
    int64_t result;
    void *value;
} SyscallResultP;

#define SYSCALL_OK 0
#define ANOS_MAP_VIRTUAL_FLAG_READ 1
#define ANOS_MAP_VIRTUAL_FLAG_WRITE 2
#define MAX_IPC_BUFFER_SIZE 4096
#define SYS_VFS_TAG_GET_SIZE 1
#define SYS_VFS_TAG_LOAD_PAGE 2

SyscallResult anos_find_named_channel(const char *name) {
    (void)name;
    SyscallResult result;
    if (mock_find_named_channel_should_fail) {
        result.result = -1;
        result.value = 0;
    } else {
        result.result = SYSCALL_OK;
        result.value = 12345; // Mock channel cookie
    }
    return result;
}

SyscallResultP anos_map_virtual(size_t size, uintptr_t addr, int flags) {
    (void)size;
    (void)addr;
    (void)flags;
    SyscallResultP result;
    if (mock_map_virtual_should_fail) {
        result.result = -1;
        result.value = NULL;
    } else {
        result.result = SYSCALL_OK;
        mock_mapped_memory = malloc(size); // Mock mapped memory
        result.value = mock_mapped_memory;
    }
    return result;
}

static char mock_file_content[1024] = "";
static size_t mock_file_size = 0;
static int mock_file_load_should_fail = 0;

SyscallResult anos_send_message(uint64_t cookie, uint64_t tag, size_t size, void *buffer) {
    (void)size;
    (void)buffer;

    SyscallResult result;
    if (mock_send_message_should_fail) {
        result.result = -1;
        result.value = 0;
        return result;
    }

    result.result = SYSCALL_OK;

    // First call: sys_vfs_cookie=12345, tag=1 -> returns ramfs cookie
    if (cookie == 12345 && tag == 1) { // Find filesystem driver
        result.value = 67890;          // Mock ramfs cookie
        // Second call: ramfs_cookie=67890, tag=1 -> returns file size
    } else if (cookie == 67890 && tag == SYS_VFS_TAG_GET_SIZE) { // Get file size
        result.value = mock_file_size;
        // Third call: ramfs_cookie=67890, tag=2 -> returns loaded data
    } else if (cookie == 67890 && tag == SYS_VFS_TAG_LOAD_PAGE) { // Load page
        if (mock_file_load_should_fail) {
            result.value = 0;
        } else {
            // Copy mock file content to buffer
            char *buf = (char *)buffer;
            size_t copy_size = mock_file_size > MAX_IPC_BUFFER_SIZE ? MAX_IPC_BUFFER_SIZE : mock_file_size;
            memcpy(buf, mock_file_content, copy_size);
            result.value = copy_size;
        }
    }

    return result;
}

SyscallResult anos_unmap_virtual(size_t size, uintptr_t addr) {
    (void)size;
    (void)addr;
    if (mock_mapped_memory) {
        free(mock_mapped_memory);
        mock_mapped_memory = NULL;
    }
    SyscallResult result;
    result.result = SYSCALL_OK;
    result.value = 0;
    return result;
}

/* Mock process creation function */
static int mock_create_server_process_should_fail = 0;
static int64_t mock_created_pid = 100;

int64_t create_server_process(uint64_t stack_size, size_t caps_count, const void *caps, size_t args_count,
                              const char **args) {
    (void)stack_size;
    (void)caps_count;
    (void)caps;
    (void)args_count;
    (void)args;

    if (mock_create_server_process_should_fail) {
        return -1;
    }

    return mock_created_pid++;
}

/* Mock syscall capabilities array */
uint64_t __syscall_capabilities[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                                     13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};

/* Test helper functions */
static void reset_mocks(void) {
    mock_json_loads_should_fail = 0;
    mock_find_named_channel_should_fail = 0;
    mock_map_virtual_should_fail = 0;
    mock_send_message_should_fail = 0;
    mock_file_load_should_fail = 0;
    mock_create_server_process_should_fail = 0;
    mock_file_size = 0;
    mock_created_pid = 100;
    if (mock_mapped_memory) {
        free(mock_mapped_memory);
        mock_mapped_memory = NULL;
    }
    memset(mock_file_content, 0, sizeof(mock_file_content));
    mock_json_root = NULL;
}

static json_t *create_mock_string(const char *value) {
    json_t *json = malloc(sizeof(json_t));
    json->type = JSON_STRING;
    json->data.string_value = value;
    json->size = 0;
    return json;
}

static json_t *create_mock_integer(int64_t value) {
    json_t *json = malloc(sizeof(json_t));
    json->type = JSON_INTEGER;
    json->data.integer_value = value;
    json->size = 0;
    return json;
}

static json_t *create_mock_array(json_t **items, size_t count) {
    json_t *json = malloc(sizeof(json_t));
    json->type = JSON_ARRAY;
    json->data.array_items = items;
    json->size = count;
    return json;
}

static json_t *create_mock_object(const char **keys, json_t **values, size_t count) {
    json_t *json = malloc(sizeof(json_t));
    json->type = JSON_OBJECT;
    json->data.object.keys = keys;
    json->data.object.values = values;
    json->size = count;
    return json;
}

/* Test fixtures */
static void *config_setup(const MunitParameter params[], void *user_data) {
    (void)params;
    (void)user_data;
    reset_mocks();
    return NULL;
}

static void config_teardown(void *fixture) {
    (void)fixture;
    reset_mocks();
}

/* Tests for load_config_file */
static MunitResult test_load_config_file_success(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    const char *test_content = "{ \"boot_servers\": [] }";
    strncpy(mock_file_content, test_content, sizeof(mock_file_content) - 1);
    mock_file_content[sizeof(mock_file_content) - 1] = '\0';
    mock_file_size = strlen(test_content);

    const char *result = load_config_file("test.json");

    munit_assert_not_null(result);
    munit_assert_string_equal(result, test_content);

    free((void *)result);
    return MUNIT_OK;
}

static MunitResult test_load_config_file_vfs_not_found(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_find_named_channel_should_fail = 1;

    const char *result = load_config_file("test.json");

    munit_assert_null(result);
    return MUNIT_OK;
}

static MunitResult test_load_config_file_map_virtual_fails(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_map_virtual_should_fail = 1;

    const char *result = load_config_file("test.json");

    munit_assert_null(result);
    return MUNIT_OK;
}

static MunitResult test_load_config_file_send_message_fails(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_send_message_should_fail = 1;

    const char *result = load_config_file("test.json");

    munit_assert_null(result);
    return MUNIT_OK;
}

static MunitResult test_load_config_file_load_fails(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_file_size = 100;
    mock_file_load_should_fail = 1;

    const char *result = load_config_file("test.json");

    munit_assert_null(result);
    return MUNIT_OK;
}

/* Tests for process_config */
static MunitResult test_process_config_invalid_json(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_json_loads_should_fail = 1;

    ProcessConfigResult result = process_config("invalid json");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);
    return MUNIT_OK;
}

static MunitResult test_process_config_root_not_object(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    json_t mock_root = {.type = JSON_ARRAY, .size = 0};
    mock_json_root = &mock_root;

    ProcessConfigResult result = process_config("[]");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);
    return MUNIT_OK;
}

static MunitResult test_process_config_no_boot_servers(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    const char *keys[] = {"other_key"};
    json_t *values[] = {create_mock_string("value")};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_OK);

    free(values[0]);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_boot_servers_not_array(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {create_mock_string("not an array")};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_OK);

    free(values[0]);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_server_not_object(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    json_t *array_items[] = {create_mock_string("not an object")};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);

    free(array_items[0]);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_missing_required_fields(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    // Server object missing required fields
    const char *server_keys[] = {"other_field"};
    json_t *server_values[] = {create_mock_string("value")};
    json_t *server_obj = create_mock_object(server_keys, server_values, 1);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);

    free(server_values[0]);
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_invalid_field_types(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    // Server object with invalid field types
    const char *server_keys[] = {"name", "path", "stack_size"};
    json_t *server_values[] = {create_mock_integer(123), // name should be string
                               create_mock_string("/path/to/server"), create_mock_integer(8192)};
    json_t *server_obj = create_mock_object(server_keys, server_values, 3);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);

    for (int i = 0; i < 3; i++) {
        free(server_values[i]);
    }
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_process_creation_failure(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    mock_create_server_process_should_fail = 1;

    // Valid server object
    const char *server_keys[] = {"name", "path", "stack_size"};
    json_t *server_values[] = {create_mock_string("test_server"), create_mock_string("/path/to/server"),
                               create_mock_integer(8192)};
    json_t *server_obj = create_mock_object(server_keys, server_values, 3);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_FAILURE);

    for (int i = 0; i < 3; i++) {
        free(server_values[i]);
    }
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_success(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    // Valid server object with capabilities and arguments
    json_t *cap1 = create_mock_string("SYSCALL_DEBUG_PRINT");
    json_t *cap2 = create_mock_string("SYSCALL_CREATE_THREAD");
    json_t *caps_items[] = {cap1, cap2};
    json_t *caps_array = create_mock_array(caps_items, 2);

    json_t *arg1 = create_mock_string("--verbose");
    json_t *arg2 = create_mock_string("--config=/etc/test.conf");
    json_t *args_items[] = {arg1, arg2};
    json_t *args_array = create_mock_array(args_items, 2);

    const char *server_keys[] = {"name", "path", "stack_size", "capabilities", "arguments"};
    json_t *server_values[] = {create_mock_string("test_server"), create_mock_string("/path/to/server"),
                               create_mock_integer(8192), caps_array, args_array};
    json_t *server_obj = create_mock_object(server_keys, server_values, 5);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_OK);

    // Cleanup
    free(cap1);
    free(cap2);
    free(caps_array);
    free(arg1);
    free(arg2);
    free(args_array);
    for (int i = 0; i < 3; i++) {
        free(server_values[i]);
    }
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_invalid_capability(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    // Invalid capability name
    json_t *cap1 = create_mock_string("INVALID_SYSCALL");
    json_t *caps_items[] = {cap1};
    json_t *caps_array = create_mock_array(caps_items, 1);

    const char *server_keys[] = {"name", "path", "stack_size", "capabilities"};
    json_t *server_values[] = {create_mock_string("test_server"), create_mock_string("/path/to/server"),
                               create_mock_integer(8192), caps_array};
    json_t *server_obj = create_mock_object(server_keys, server_values, 4);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);

    free(cap1);
    free(caps_array);
    for (int i = 0; i < 3; i++) {
        free(server_values[i]);
    }
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

static MunitResult test_process_config_non_string_capability(const MunitParameter params[], void *fixture) {
    (void)params;
    (void)fixture;

    // Capability that's not a string
    json_t *cap1 = create_mock_integer(123);
    json_t *caps_items[] = {cap1};
    json_t *caps_array = create_mock_array(caps_items, 1);

    const char *server_keys[] = {"name", "path", "stack_size", "capabilities"};
    json_t *server_values[] = {create_mock_string("test_server"), create_mock_string("/path/to/server"),
                               create_mock_integer(8192), caps_array};
    json_t *server_obj = create_mock_object(server_keys, server_values, 4);

    json_t *array_items[] = {server_obj};
    json_t *boot_servers_array = create_mock_array(array_items, 1);

    const char *keys[] = {"boot_servers"};
    json_t *values[] = {boot_servers_array};
    mock_json_root = create_mock_object(keys, values, 1);

    ProcessConfigResult result = process_config("{}");

    munit_assert_int(result, ==, PROCESS_CONFIG_INVALID);

    free(cap1);
    free(caps_array);
    for (int i = 0; i < 3; i++) {
        free(server_values[i]);
    }
    free(server_obj);
    free(boot_servers_array);
    free(mock_json_root);
    return MUNIT_OK;
}

/* Test suite definition */
static MunitTest config_tests[] = {
        {"/load_config_file/success", test_load_config_file_success, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/load_config_file/vfs_not_found", test_load_config_file_vfs_not_found, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/load_config_file/map_virtual_fails", test_load_config_file_map_virtual_fails, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/load_config_file/send_message_fails", test_load_config_file_send_message_fails, config_setup,
         config_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/load_config_file/load_fails", test_load_config_file_load_fails, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/invalid_json", test_process_config_invalid_json, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/root_not_object", test_process_config_root_not_object, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/no_boot_servers", test_process_config_no_boot_servers, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/boot_servers_not_array", test_process_config_boot_servers_not_array, config_setup,
         config_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/server_not_object", test_process_config_server_not_object, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/missing_required_fields", test_process_config_missing_required_fields, config_setup,
         config_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/invalid_field_types", test_process_config_invalid_field_types, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/process_creation_failure", test_process_config_process_creation_failure, config_setup,
         config_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/success", test_process_config_success, config_setup, config_teardown, MUNIT_TEST_OPTION_NONE,
         NULL},
        {"/process_config/invalid_capability", test_process_config_invalid_capability, config_setup, config_teardown,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/process_config/non_string_capability", test_process_config_non_string_capability, config_setup,
         config_teardown, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite config_suite = {"/config", config_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&config_suite, NULL, argc, argv);
}