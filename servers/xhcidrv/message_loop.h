/*
* Message loop for xHCI Driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdnoreturn.h>

#ifndef ANOS_XHCI_MESSAGE_LOOP_H
#define ANOS_XHCI_MESSAGE_LOOP_H

noreturn void xhci_message_loop(uint64_t xhci_channel);

#endif //ANOS_XHCI_MESSAGE_LOOP_H
