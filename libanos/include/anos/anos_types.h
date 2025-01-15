/*
 * Anos system types
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#ifndef __ANOS_ANOS_TYPES_H
#define __ANOS_ANOS_TYPES_H

#include <stdint.h>

typedef struct {
    uint64_t physical_total;
    uint64_t physical_avail;
} AnosMemInfo;

#endif //__ANOS_ANOS_TYPES_H
