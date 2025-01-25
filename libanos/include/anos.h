/*
 * Top level include for Anos user-mode software
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_ANOS_H
#define __ANOS_ANOS_H

#include <stdint.h>

const char *libanos_version();

void anos_task_sleep_current_secs(uint64_t secs);

#endif //__ANOS_ANOS_H
