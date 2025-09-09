/*
 * Message loop for xHCI Driver
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>

#include <anos/syscalls.h>

#ifdef DEBUG_XHCI_OPS
#define ops_debugf(...) printf(__VA_ARGS__)
#ifdef VERY_NOISY_XHCI_OPS
#define ops_vdebugf(...) printf(__VA_ARGS__)
#else
#define ops_vdebugf(...)
#endif
#else
#define ops_debugf(...)
#define ops_vdebugf(...)
#endif

static void handle_usb_message(const uint64_t msg_cookie, const uint64_t tag,
                               void *buffer, const size_t buffer_size) {
    ops_vdebugf("xHCI: Received message with tag %lu, size %lu\n", tag,
                buffer_size);

    // For now, just acknowledge any messages
    // TODO: Implement proper USB device communication
    anos_reply_message(msg_cookie, 0);
}

noreturn void xhci_message_loop(const uint64_t xhci_channel) {
    while (1) {
        void *ipc_buffer = (void *)0x300000000;
        size_t buffer_size = 4096;
        uint64_t tag = 0;

        const SyscallResult recv_result =
                anos_recv_message(xhci_channel, &tag, &buffer_size, ipc_buffer);

        const uint64_t msg_cookie = recv_result.value;

        if (recv_result.result == SYSCALL_OK && msg_cookie) {
            ops_vdebugf("xHCI: Received message with tag %lu, size %lu\n", tag,
                        buffer_size);
            handle_usb_message(msg_cookie, tag, ipc_buffer, buffer_size);
        } else {
            ops_debugf("xHCI: Error receiving message [0x%016lx]\n",
                       (uint64_t)recv_result.result);

            // Sleep briefly to avoid pegging the CPU if we're in an error loop
            anos_task_sleep_current_secs(1);
        }
    }
}
