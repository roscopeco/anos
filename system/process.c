/*
 * SYSTEM process management routines
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <anos/syscalls.h>
#include <anos/types.h>

#include "loader.h"
#include "process.h"

#define VM_PAGE_SIZE ((0x1000))

#ifndef strnlen
// newlib doesn't declare this, but it is defined...
int strnlen(const char *param, int maxlen);
#endif

extern void *_code_start;
extern void *_code_end;
extern void *_bss_start;
extern void *_bss_end;
extern void *__user_stack_top;

// TODO Replace these once malloc is good, this is not thread safe!
static char __attribute__((aligned(VM_PAGE_SIZE))) newstack_p[VM_PAGE_SIZE];
static char __attribute__((aligned(VM_PAGE_SIZE))) argv_p[VM_PAGE_SIZE];

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

    if (!build_new_process_init_values(STACK_TOP, capc, capv, argc, argv,
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

static char __attribute__((aligned(VM_PAGE_SIZE))) argv_p[4096];
static char __attribute__((aligned(VM_PAGE_SIZE))) newstack_p[4096];
