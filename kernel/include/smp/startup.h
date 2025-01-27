/*
 * stage3 - SMP startup support
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 */

#ifndef __ANOS_SMP_STARTUP_H
#define __ANOS_SMP_STARTUP_H

#include <stdint.h>

void smp_bsp_start_aps(uint32_t volatile *lapic);
void smp_bsp_start_ap(uint8_t ap_id, uint32_t volatile *lapic);

#endif //__ANOS_SMP_STARTUP_H
