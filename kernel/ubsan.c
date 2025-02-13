/*
 * stage3 - Undefined Behaviour Sanitizer support
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is only built during a CONSERVATIVE_BUILD
 */

#include <stdint.h>

#include "debugprint.h"
#include "panic.h"
#include "printdec.h"
#include "printhex.h"

#define is_aligned(value, alignment) !(value & (alignment - 1))

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
    TypeDescriptor lhs;
    TypeDescriptor rhs;
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

static void log_location(SourceLocation *location) {
    debugstr("    file: ");
    debugstr(location->file);
    debugstr("\n    line: ");
    printdec(location->line, debugchar);
    debugstr("\n    column: ");
    printdec(location->column, debugchar);
    debugstr("\n");
}

void __ubsan_handle_type_mismatch_v1(TypeMismatchInfo *type_mismatch,
                                     uintptr_t pointer) {
    uintptr_t alignment = (uintptr_t)1ULL << type_mismatch->alignment;
    char *violation = "type mismatch";

    if (!pointer) {
        debugstr("!!!!! UBSAN: null dereference");
    } else if (alignment && !is_aligned(pointer, alignment)) {
        debugstr("!!!!! UBSAN: unaligned access");
    } else {
        debugstr("!!!!! UBSAN: type mismatch: ");
        debugstr(type_check_kinds[type_mismatch->type_check_kind]);
        debugstr(" address ");
        printhex64(pointer, debugchar);
        debugstr(" with insufficient space for object of type ");
        debugstr(type_mismatch->type->name);
    }

    debugstr("\n");
    log_location(&type_mismatch->location);

#ifdef CONSERVATIVE_PANICKY
    panic("[BUG] Undefined behaviour encountered");
#endif
}

void __ubsan_handle_shift_out_of_bounds(OutOfBoundsInfo *oob_info, uint64_t lhs,
                                        uint64_t rhs) {
    debugstr("!!!!! UBSAN: Shift out-of-bounds: ");
    printhex64(lhs, debugchar);
    debugstr(" << ");
    printhex64(rhs, debugchar);
    debugstr("\n");

    log_location(&oob_info->location);

#ifdef CONSERVATIVE_PANICKY
    panic("[BUG] Undefined behaviour encountered");
#endif
}

// TODO The following need to be implemented properly...
//
void __ubsan_handle_add_overflow(void) {
    panic("[BUG] Undefined behaviour encountered (add_overflow: debug not yet "
          "implemented)");
}

void __ubsan_handle_mul_overflow(void) {
    panic("[BUG] Undefined behaviour encountered (mul_overflow: debug not yet "
          "implemented)");
}

void __ubsan_handle_divrem_overflow(void) {
    panic("[BUG] Undefined behaviour encountered (divrem_overflow: debug not "
          "yet implemented)");
}

void __ubsan_handle_out_of_bounds(void) {
    panic("[BUG] Undefined behaviour encountered (out_of_bounds: debug not yet "
          "implemented)");
}

void __ubsan_handle_pointer_overflow(void) {
    panic("[BUG] Undefined behaviour encountered (pointer_overflow: debug not "
          "yet implemented)");
}

void __ubsan_handle_builtin_unreachable(void) {
    panic("[BUG] Undefined behaviour encountered (builtin_unreachable: debug "
          "not yet implemented)");
}

void __ubsan_handle_load_invalid_value(void) {
    panic("[BUG] Undefined behaviour encountered (load_invalid_value: debug "
          "not yet implemented)");
}

void __ubsan_handle_invalid_builtin(void) {
    panic("[BUG] Undefined behaviour encountered (invalid_builtin: debug not "
          "yet implemented)");
}