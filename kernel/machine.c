/*
 * stage3 - Kernel entry point
 * anos - An Operating System
 *
 * Copyright (c) 2023 Ross Bamford
 *
 * Generally-useful machine-related routines
 */

#include <stdbool.h>
#include <stdnoreturn.h>

noreturn void halt_and_catch_fire() {
  __asm__ volatile("cli\n\t");

  while (true) {
    __asm__ volatile("hlt\n\t");
  }
}
