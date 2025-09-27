/*
 * Mock jansson.h for SYSTEM testing
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#ifndef __JANSSON_H
#define __JANSSON_H

#include <stddef.h>
#include <stdint.h>

/* Forward declaration - actual implementation in test file */
typedef struct json_t json_t;

/* JSON error structure */
typedef struct {
    int line;
    char text[256];
} json_error_t;

/* JSON loading flags */
#define JSON_DECODE_ANY 0

/* Mock function declarations - implemented in test file */
json_t *json_loads(const char *input, size_t flags, json_error_t *error);
int json_is_object(const json_t *json);
int json_is_array(const json_t *json);
int json_is_string(const json_t *json);
int json_is_integer(const json_t *json);
json_t *json_object_get(const json_t *object, const char *key);
json_t *json_array_get(const json_t *array, size_t index);
size_t json_array_size(const json_t *array);
const char *json_string_value(const json_t *string);
int64_t json_integer_value(const json_t *integer);
void json_decref(json_t *json);

#endif /* __JANSSON_H */