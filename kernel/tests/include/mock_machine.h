/*
 * Test interface to mock machine.c implementation for tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_MACHINE_H
#define __ANOS_TESTS_TEST_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

static const uint64_t TEST_IRQ_DISABLE_COOKIE = 0x123456789abcdef0;

void mock_machine_reset(void);
inline bool mock_machine_outl_avail(uint16_t port);
uint32_t mock_machine_read_outl_buffer(uint16_t port);
bool mock_machine_write_outl_buffer(uint16_t port, uint32_t value);
inline bool mock_machine_inl_avail(uint16_t port);
uint32_t mock_machine_read_inl_buffer(uint16_t port);
bool mock_machine_write_inl_buffer(uint16_t port, uint32_t value);
uint32_t mock_machine_intr_disable_level();
uint32_t mock_machine_max_intr_disable_level();

#endif //__ANOS_TESTS_TEST_MACHINE_H