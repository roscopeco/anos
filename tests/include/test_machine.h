/*
 * Test interface to mock machine.c implementation for tests
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_TESTS_TEST_MACHINE_H
#define __ANOS_TESTS_TEST_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

void test_machine_reset(void);
inline bool test_machine_outl_avail(uint16_t port);
uint32_t test_machine_read_outl_buffer(uint16_t port);
bool test_machine_write_outl_buffer(uint16_t port, uint32_t value);
inline bool test_machine_inl_avail(uint16_t port);
uint32_t test_machine_read_inl_buffer(uint16_t port);
bool test_machine_write_inl_buffer(uint16_t port, uint32_t value);

#endif //__ANOS_TESTS_TEST_MACHINE_H