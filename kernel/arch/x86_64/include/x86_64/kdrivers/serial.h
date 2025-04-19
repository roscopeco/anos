/*
 * stage3 - Kernel serial driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_KERNEL_DRIVERS_SERIAL_H
#define __ANOS_KERNEL_DRIVERS_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SERIAL_PORT_DUMMY = 0,
    SERIAL_PORT_COM1 = 0x3f8,
    SERIAL_PORT_COM2 = 0x2f8,
} __attribute__((packed)) SerialPort;

bool serial_init(SerialPort port);
bool serial_available(SerialPort port);
char serial_recvchar(SerialPort port);
bool serial_tx_empty(SerialPort port);
void serial_sendchar(SerialPort port, char a);

#endif //__ANOS_KERNEL_DRIVERS_SERIAL_H