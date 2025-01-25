/*
 * stage3 - Kernel Configuration
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * **This is not a dumping-ground for common code!**
 */

#ifndef __ANOS_KERNEL_CONFIG_H
#define __ANOS_KERNEL_CONFIG_H

/* ********************************************************** */
/* Top-level configuration - edit these to change stuff */

// Frequency (in Hz, obvs) at which the main kernel tick should
// run. This will always be _somewhat_ approximate depending on
// the timer that's selected to drive it.
#define KERNEL_HZ 100

/* ********************************************************** */
/* 
 * Derived configuration - values that are derived from the
 * top-level variables and that are still broadly applicable.
 * 
 * _Usually_ there's no need to edit these...
 */

// Number of nanoseconds per kernel tick. Used to derive sleep
// delays etc.
#define NANOS_PER_TICK (((1000000000 / (KERNEL_HZ))))

#endif //__ANOS_KERNEL_CONFIG_H