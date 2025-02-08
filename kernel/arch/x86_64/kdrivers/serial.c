/*
 * stage3 - Kernel serial driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>

#include "kdrivers/serial.h"
#include "machine.h"

bool serial_init(SerialPort port) {
    outb(port + 1, 0x00); // Disable all interrupts
    outb(port + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(port + 0, 0x01); // Set divisor to 3 (lo byte) 115200 baud
    outb(port + 1, 0x00); //                  (hi byte)
    outb(port + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(port + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(port + 4, 0x0B); // IRQs enabled, RTS/DSR set
    outb(port + 4, 0x1E); // Set in loopback mode, test the serial chip
    outb(port + 0,
         0xAE); // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (inb(port + 0) != 0xAE) {
        return false;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(port + 4, 0x0F);
    return true;
}

bool serial_available(SerialPort port) {
    if (port == SERIAL_PORT_DUMMY) {
        return false;
    }

    return inb(port + 5) & 1;
}

char serial_recvchar(SerialPort port) {
    if (port == SERIAL_PORT_DUMMY) {
        return 0;
    }

    while (serial_available(port) == 0)
        ;

    return inb(port);
}

bool serial_tx_empty(SerialPort port) {
    if (port == SERIAL_PORT_DUMMY) {
        return false;
    }

    return inb(port + 5) & 0x20;
}

void serial_sendchar(SerialPort port, char a) {
    if (port == SERIAL_PORT_DUMMY) {
        return;
    }

    while (serial_tx_empty(port) == 0)
        ;

    outb(port, a);
}