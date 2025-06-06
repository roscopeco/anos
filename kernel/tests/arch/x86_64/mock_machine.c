/*
 * Mock implementation of machine.c for hosted tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

#include "mock_machine.h"
#include "munit.h"

#include "munit.h"

#define BUFFER_SIZE 1024
#define BUFFER_MASK ((BUFFER_SIZE - 1))

// 128MiB of ring buffers, could be more efficient but whatever, this is test
// code...
static uint32_t out_buffer[65536][1024];
static uint32_t in_buffer[65536][1024];

static uint32_t in_buffer_read_ptr[65536];
static uint32_t in_buffer_write_ptr[65536];
static uint32_t out_buffer_read_ptr[65536];
static uint32_t out_buffer_write_ptr[65536];

static uint32_t intr_disable_level;
static uint32_t max_intr_disable_level;

static bool waited_for_interrupt;

void mock_machine_reset(void) {
    for (int i = 0; i < 65536; i++) {
        in_buffer_read_ptr[i] = 0;
        in_buffer_write_ptr[i] = 0;
        out_buffer_read_ptr[i] = 0;
        out_buffer_write_ptr[i] = 0;
    }

    intr_disable_level = 0;
    max_intr_disable_level = 0;
    waited_for_interrupt = false;
}

inline bool mock_machine_outl_avail(uint16_t port) {
    return out_buffer_read_ptr[port] != out_buffer_write_ptr[port];
}

uint32_t mock_machine_read_outl_buffer(uint16_t port) {
    if (!mock_machine_outl_avail(port)) {
        return 0;
    }

    uint32_t result = out_buffer[port][out_buffer_read_ptr[port]++];
    out_buffer_read_ptr[port] &= BUFFER_MASK;
    return result;
}

bool mock_machine_write_outl_buffer(uint16_t port, uint32_t value) {
    if ((out_buffer_write_ptr[port] == out_buffer_read_ptr[port] - 1) ||
        (in_buffer_write_ptr[port] == 0xffff &&
         in_buffer_read_ptr[port] == 0)) {
        return false;
    }

    out_buffer[port][out_buffer_write_ptr[port]++] = value;
    out_buffer_write_ptr[port] &= BUFFER_MASK;

    return true;
}

inline bool mock_machine_inl_avail(uint16_t port) {
    return in_buffer_read_ptr[port] != in_buffer_write_ptr[port];
}

uint32_t mock_machine_read_inl_buffer(uint16_t port) {
    if (!mock_machine_inl_avail(port)) {
        return 0;
    }

    uint32_t result = in_buffer[port][in_buffer_read_ptr[port]++];
    in_buffer_read_ptr[port] &= BUFFER_MASK;
    return result;
}

bool mock_machine_write_inl_buffer(uint16_t port, uint32_t value) {
    if ((in_buffer_write_ptr[port] == in_buffer_read_ptr[port] - 1) ||
        (in_buffer_write_ptr[port] == 0xffff &&
         in_buffer_read_ptr[port] == 0)) {
        return false;
    }

    in_buffer[port][in_buffer_write_ptr[port]++] = value;
    in_buffer_write_ptr[port] &= BUFFER_MASK;

    return true;
}

/* Implementation of machine.c interface */

void halt_and_catch_fire(void) { exit(100); }

void outl(uint16_t port, uint32_t value) {
    if (!mock_machine_write_outl_buffer(port, value)) {
        fprintf(stderr,
                "WARN: mock_machine outl [port 0x%04x value 0x%8x] discarded: "
                "buffer full\n",
                port, value);
    }
}

uint32_t inl(uint16_t port) {
    if (!mock_machine_inl_avail(port)) {
        fprintf(stderr,
                "WARN: mock_machine inll [port 0x%04x] underflow: buffer "
                "empty\n",
                port);
        return 0;
    }

    return mock_machine_read_inl_buffer(port);
}

void enable_interrupts() {
    if (intr_disable_level > 0) {
        --intr_disable_level;
    }
}

void disable_interrupts() {
    ++intr_disable_level;

    if (intr_disable_level > max_intr_disable_level) {
        max_intr_disable_level = intr_disable_level;
    }
}

uint64_t save_disable_interrupts(void) {
    ++intr_disable_level;

    if (intr_disable_level > max_intr_disable_level) {
        max_intr_disable_level = intr_disable_level;
    }

    return 0xdeadbeef;
}

void restore_saved_interrupts(uint64_t state) {
    munit_assert_uint64(state, ==, 0xdeadbeef);
    --intr_disable_level;
}

uint32_t mock_machine_intr_disable_level() { return intr_disable_level; }

uint32_t mock_machine_max_intr_disable_level() {
    return max_intr_disable_level;
}

void wait_for_interrupt(void) { waited_for_interrupt = true; }