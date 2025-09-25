/*
 * Undefined Behaviour Sanitizer support for usermode
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 *
 * This is only built during a CONSERVATIVE_BUILD
 */

#include <stdint.h>
#include <stdio.h>

#include <anos/syscalls.h>

#define is_aligned(value, alignment) !(value & (alignment - 1))

#define ubsan_panic(msg)                                                                                               \
    do {                                                                                                               \
        printf("%s\n", msg);                                                                                           \
        anos_kill_current_task();                                                                                      \
    } while (0)

typedef struct {
    char *file;
    uint32_t line;
    uint32_t column;
} SourceLocation;

typedef struct {
    uint16_t kind;
    uint16_t info;
    char name[];
} TypeDescriptor;

typedef struct {
    SourceLocation location;
    TypeDescriptor *type;
    uint8_t alignment;
    uint8_t type_check_kind;
} TypeMismatchInfo;

typedef struct {
    SourceLocation location;
    TypeDescriptor *lhs;
    TypeDescriptor *rhs;
} OutOfBoundsInfo;

static char *type_check_kinds[] = {
        "load of",
        "store to",
        "reference binding to",
        "member access within",
        "member call on",
        "constructor call on",
        "downcast of",
        "downcast of",
        "upcast of",
        "cast to virtual base of",
};

static void log_location(const SourceLocation *location) {
    printf("    file: %s\n    line: %d\n    column: %d\n", location->file, location->line, location->column);
}

void __ubsan_handle_type_mismatch_v1(const TypeMismatchInfo *type_mismatch, const uintptr_t pointer) {
    const uintptr_t alignment = (uintptr_t)1ULL << type_mismatch->alignment;

    if (!pointer) {
        printf("!!!!! UBSAN: null dereference");
    } else if (alignment && !is_aligned(pointer, alignment)) {
        printf("!!!!! UBSAN: unaligned access");
    } else {
        printf("!!!!! UBSAN: type mismatch: %s address %016lx with "
               "insufficient space for object of type %s\n",
               type_check_kinds[type_mismatch->type_check_kind], pointer, type_mismatch->type->name);
    }

    log_location(&type_mismatch->location);

    ubsan_panic("[BUG] Undefined behaviour encountered");
}

void __ubsan_handle_shift_out_of_bounds(const OutOfBoundsInfo *oob_info, const uint64_t lhs, const uint64_t rhs) {
    printf("!!!!! UBSAN: Shift out-of-bounds: %016lx << 0x%016lx\n", lhs, rhs);

    log_location(&oob_info->location);

    ubsan_panic("[BUG] Undefined behaviour encountered");
}

// TODO The following need to be implemented properly...
//
void __ubsan_handle_add_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (add_overflow: "
                "debug not yet implemented)");
}

void __ubsan_handle_sub_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (sub_overflow: "
                "debug not yet implemented)");
}

void __ubsan_handle_mul_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (mul_overflow: "
                "debug not yet implemented)");
}

void __ubsan_handle_divrem_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (divrem_overflow: debug not "
                "yet implemented)");
}

void __ubsan_handle_negate_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (negate_overflow: "
                "debug not yet implemented)");
}

void __ubsan_handle_out_of_bounds(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (out_of_bounds: "
                "debug not yet "
                "implemented)");
}

void __ubsan_handle_pointer_overflow(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered "
                "(pointer_overflow: debug not "
                "yet implemented)");
}

void __ubsan_handle_builtin_unreachable(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (builtin_unreachable: debug "
                "not yet implemented)");
}

void __ubsan_handle_load_invalid_value(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (load_invalid_value: debug "
                "not yet implemented)");
}

void __ubsan_handle_invalid_builtin(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered (invalid_builtin: debug not "
                "yet implemented)");
}

void __ubsan_handle_vla_bound_not_positive(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered "
                "(vla_bound_not_positive: debug not yet implemented)");
}

void __ubsan_handle_nonnull_arg(void) {
    ubsan_panic("[BUG] Undefined behaviour encountered "
                "(nonnull_arg: debug not yet implemented)");
}