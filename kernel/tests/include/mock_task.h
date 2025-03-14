/*
 * Mocks for testing with tasks
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_TESTS_TEST_TASK_H
#define __ANOS_TESTS_TEST_TASK_H

#include "task.h"

void mock_task_set_curent(Task *new_current);
Process *mock_task_get_last_create_new_owner(void);
uintptr_t mock_task_get_last_create_new_sp(void);
uintptr_t mock_task_get_last_create_new_sys_ssp(void);
uintptr_t mock_task_get_last_create_new_bootstrap(void);
uintptr_t mock_task_get_last_create_new_func(void);
TaskClass mock_task_get_last_create_new_class(void);

#endif //__ANOS_TESTS_TEST_TASK_H