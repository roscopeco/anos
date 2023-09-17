/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdnoreturn.h>

#ifndef __ANOS_KERNEL_MACHINE_H
#define __ANOS_KERNEL_MACHINE_H

noreturn void halt_and_catch_fire();

#endif//__ANOS_KERNEL_MACHINE_H