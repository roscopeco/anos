/*
 * A test server that stress tests the kernel syscalls
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <anos/syscalls.h>
#include <anos/types.h>

#define TEST_PASS 0
#define TEST_FAIL 1

#define VM_TEST_BASE 0x20000000UL
#define VM_TEST_SIZE 0x1000
#define REGION_TEST_BASE 0x30000000UL
#define MAX_TEST_REGIONS 100
#define MAX_TEST_MAPPINGS 50

#define MAX_TEST_CHANNELS 20
#define MAX_TEST_MESSAGES 100
#define TEST_MESSAGE_SIZE 256
#define MAX_NAMED_CHANNELS 50

static int test_failures = 0;
static int test_passes = 0;

#define TEST_ASSERT(condition, msg)                                            \
    do {                                                                       \
        if (!(condition)) {                                                    \
            anos_kprint("[FAIL] ");                                            \
            anos_kprint(msg);                                                  \
            anos_kprint("\n");                                                 \
            test_failures++;                                                   \
            return TEST_FAIL;                                                  \
        } else {                                                               \
            test_passes++;                                                     \
        }                                                                      \
    } while (0)

#define RUN_TEST(test_func)                                                    \
    do {                                                                       \
        anos_kprint("Running " #test_func "... ");                             \
        if (test_func() == TEST_PASS) {                                        \
            anos_kprint("[PASS]\n");                                           \
        }                                                                      \
    } while (0)

// Memory management stress tests
static int test_basic_virtual_mapping(void) {
    // Test basic virtual memory allocation
    const SyscallResultP result = anos_map_virtual(
            VM_TEST_SIZE, VM_TEST_BASE,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    TEST_ASSERT(result.result == SYSCALL_OK, "Basic virtual mapping failed");
    TEST_ASSERT(result.value == (void *)VM_TEST_BASE,
                "Virtual mapping returned wrong address");

    // Write to the mapped memory to verify it works
    volatile uint32_t *test_ptr = (volatile uint32_t *)VM_TEST_BASE;
    *test_ptr = 0xDEADBEEF;
    TEST_ASSERT(*test_ptr == 0xDEADBEEF,
                "Memory write/read verification failed");

    // Unmap the memory
    const SyscallResult unmap_result =
            anos_unmap_virtual(VM_TEST_SIZE, VM_TEST_BASE);
    TEST_ASSERT(unmap_result.result == SYSCALL_OK,
                "Virtual memory unmapping failed");

    return TEST_PASS;
}

static int test_virtual_mapping_stress(void) {
    const uintptr_t base_addr = VM_TEST_BASE + 0x10000;

    // Rapidly allocate and deallocate many mappings
    for (int i = 0; i < MAX_TEST_MAPPINGS; i++) {
        const uintptr_t addr =
                base_addr + (i * VM_TEST_SIZE * 2); // Avoid overlaps

        const SyscallResultP result = anos_map_virtual(
                VM_TEST_SIZE, addr,
                ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
        TEST_ASSERT(result.result == SYSCALL_OK, "Stress test mapping failed");

        // Write a pattern to verify mapping works
        volatile uint32_t *test_ptr = (volatile uint32_t *)addr;
        *test_ptr = 0x12340000 + i;
        TEST_ASSERT(*test_ptr == (0x12340000 + i),
                    "Stress test memory verification failed");

        // Immediately unmap
        const SyscallResult unmap_result =
                anos_unmap_virtual(VM_TEST_SIZE, addr);
        TEST_ASSERT(unmap_result.result == SYSCALL_OK,
                    "Stress test unmapping failed");
    }

    return TEST_PASS;
}

static int test_invalid_virtual_mappings(void) {
    // Test mapping with invalid address (kernel space)
    SyscallResultP result = anos_map_virtual(VM_TEST_SIZE, 0xFFFF800000000000UL,
                                             ANOS_MAP_VIRTUAL_FLAG_READ);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Kernel space mapping should fail");

    // Test mapping with zero size
    result = anos_map_virtual(0, VM_TEST_BASE, ANOS_MAP_VIRTUAL_FLAG_READ);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Zero size mapping should fail");

    // Test mapping with unaligned address
    result = anos_map_virtual(VM_TEST_SIZE, VM_TEST_BASE + 1,
                              ANOS_MAP_VIRTUAL_FLAG_READ);
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Unaligned address should be page-aligned internally");
    if (result.result == SYSCALL_OK) {
        anos_unmap_virtual(VM_TEST_SIZE, VM_TEST_BASE); // Clean up
    }

    return TEST_PASS;
}

static int test_basic_region_management(void) {
    constexpr uintptr_t start = REGION_TEST_BASE;
    constexpr uintptr_t end = start + VM_TEST_SIZE;

    // Create a basic region
    SyscallResult result = anos_create_region(start, end, 0);
    TEST_ASSERT(result.result == SYSCALL_OK, "Basic region creation failed");

    // Destroy the region
    result = anos_destroy_region(start);
    TEST_ASSERT(result.result == SYSCALL_OK, "Basic region destruction failed");

    return TEST_PASS;
}

static int test_region_coalescing(void) {
    constexpr uintptr_t base = REGION_TEST_BASE + 0x10000;

    // Create first region
    SyscallResult result = anos_create_region(base, base + VM_TEST_SIZE, 0);
    TEST_ASSERT(result.result == SYSCALL_OK, "First region creation failed");

    // Create adjacent region (should coalesce)
    result = anos_create_region(base + VM_TEST_SIZE, base + (VM_TEST_SIZE * 2),
                                0);
    TEST_ASSERT(result.result == SYSCALL_OK, "Adjacent region creation failed");

    // Clean up - destroy the (hopefully coalesced) region
    result = anos_destroy_region(base);
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Coalesced region destruction failed");

    return TEST_PASS;
}

static int test_region_overlap_rejection(void) {
    constexpr uintptr_t base = REGION_TEST_BASE + 0x20000;

    // Create first region
    SyscallResult result = anos_create_region(base, base + VM_TEST_SIZE, 0);
    TEST_ASSERT(result.result == SYSCALL_OK, "Initial region creation failed");

    // Try to create overlapping region (should fail)
    result = anos_create_region(base + (VM_TEST_SIZE / 2),
                                base + (VM_TEST_SIZE * 2), 0);
    TEST_ASSERT(result.result == SYSCALL_FAILURE,
                "Overlapping region should be rejected");

    // Clean up
    result = anos_destroy_region(base);
    TEST_ASSERT(result.result == SYSCALL_OK, "Region cleanup failed");

    return TEST_PASS;
}

static int test_region_stress(void) {
    constexpr uintptr_t base = REGION_TEST_BASE + 0x30000;

    // Create many non-overlapping regions
    for (int i = 0; i < MAX_TEST_REGIONS; i++) {
        const uintptr_t start = base + (i * VM_TEST_SIZE * 2);
        const uintptr_t end = start + VM_TEST_SIZE;

        const SyscallResult result = anos_create_region(start, end, 0);
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Stress test region creation failed");
    }

    // Destroy all regions
    for (int i = 0; i < MAX_TEST_REGIONS; i++) {
        const uintptr_t start = base + (i * VM_TEST_SIZE * 2);

        const SyscallResult result = anos_destroy_region(start);
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Stress test region destruction failed");
    }

    return TEST_PASS;
}

static int test_invalid_regions(void) {
    // Test region with invalid alignment
    SyscallResult result = anos_create_region(0x12345, 0x23456, 0);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Unaligned region should fail");

    // Test region in kernel space
    result = anos_create_region(0xFFFF800000000000UL, 0xFFFF800000001000UL, 0);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Kernel space region should fail");

    // Test region with end <= start
    result = anos_create_region(VM_TEST_BASE, VM_TEST_BASE, 0);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Zero-size region should fail");

    return TEST_PASS;
}

// IPC stress tests
static int test_basic_channel_lifecycle(void) {
    // Create a channel
    SyscallResult result = anos_create_channel();
    TEST_ASSERT(result.result == SYSCALL_OK, "Channel creation failed");
    TEST_ASSERT(result.value != 0, "Channel cookie should not be zero");

    const uint64_t channel_cookie = result.value;

    // Destroy the channel
    result = anos_destroy_channel(channel_cookie);
    TEST_ASSERT(result.result == SYSCALL_OK, "Channel destruction failed");

    return TEST_PASS;
}

// Global state for threaded message tests
static uint64_t test_channel_cookie = 0;
static volatile int message_test_ready = 0;
static volatile int message_test_complete = 0;

static void sender_thread(void) {
    // Wait for receiver to be ready
    while (!message_test_ready) {
        anos_task_sleep_current_secs(1);
    }

    // Allocate page-aligned buffer for message
    constexpr uintptr_t msg_buffer_addr = VM_TEST_BASE + 0x300000;
    const SyscallResultP map_result = anos_map_virtual(
            VM_TEST_SIZE, msg_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    if (map_result.result != SYSCALL_OK) {
        message_test_complete = -1;
        anos_kill_current_task();
    }

    // Prepare and send test message in page-aligned buffer
    char *test_message = (char *)msg_buffer_addr;
    snprintf(test_message, TEST_MESSAGE_SIZE,
             "Hello IPC World! Test message #%d", 42);

    const SyscallResult result =
            anos_send_message(test_channel_cookie, 0x1234,
                              strlen(test_message) + 1, test_message);

    // Clean up message buffer
    anos_unmap_virtual(VM_TEST_SIZE, msg_buffer_addr);

    // Signal completion (sender completes when message is delivered)
    message_test_complete = (result.result == SYSCALL_OK) ? 1 : -1;

    anos_kill_current_task();
}

static int test_basic_message_passing(void) {
    // Create a channel
    SyscallResult result = anos_create_channel();
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Channel creation for message test failed");

    test_channel_cookie = result.value;
    message_test_ready = 0;
    message_test_complete = 0;

    // Create sender thread with a small stack
    constexpr uintptr_t sender_stack = VM_TEST_BASE + 0x100000;
    const SyscallResultP map_result = anos_map_virtual(
            VM_TEST_SIZE, sender_stack,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);

    TEST_ASSERT(map_result.result == SYSCALL_OK,
                "Sender thread stack allocation failed");

    const SyscallResult thread_result = anos_create_thread(
            (ThreadFunc)sender_thread, sender_stack + VM_TEST_SIZE - 8);
    TEST_ASSERT(thread_result.result == SYSCALL_OK,
                "Sender thread creation failed");

    // Signal that receiver is ready
    message_test_ready = 1;

    // Allocate page-aligned buffer for receiving message
    constexpr uintptr_t recv_buffer_addr = VM_TEST_BASE + 0x400000;
    const SyscallResultP recv_map_result = anos_map_virtual(
            VM_TEST_SIZE, recv_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    TEST_ASSERT(recv_map_result.result == SYSCALL_OK,
                "Receive buffer allocation failed");

    // Receive the message
    char *recv_buffer = (char *)recv_buffer_addr;
    uint64_t recv_tag = 0;
    size_t recv_size = TEST_MESSAGE_SIZE;

    result = anos_recv_message(test_channel_cookie, &recv_tag, &recv_size,
                               recv_buffer);
    TEST_ASSERT(result.result == SYSCALL_OK, "Message receive failed");
    TEST_ASSERT(recv_tag == 0x1234, "Received tag mismatch");

    // Reply to the message to unblock the sender
    const uint64_t message_cookie = result.value;
    const SyscallResult reply_result =
            anos_reply_message(message_cookie, 0x4321);
    TEST_ASSERT(reply_result.result == SYSCALL_OK, "Message reply failed");

    // Now verify sender completed successfully
    TEST_ASSERT(message_test_complete == 1,
                "Sender thread did not complete successfully");

    // Verify message content
    char expected_message[TEST_MESSAGE_SIZE];
    snprintf(expected_message, sizeof(expected_message),
             "Hello IPC World! Test message #%d", 42);
    TEST_ASSERT(strcmp(expected_message, recv_buffer) == 0,
                "Received message content mismatch");

    // Clean up receive buffer
    anos_unmap_virtual(VM_TEST_SIZE, recv_buffer_addr);

    // Clean up
    anos_unmap_virtual(VM_TEST_SIZE, sender_stack);
    anos_destroy_channel(test_channel_cookie);

    return TEST_PASS;
}

// Stress test state
static volatile int stress_test_current = 0;
static volatile int stress_test_errors = 0;

static void stress_sender_thread(void) {
    while (!message_test_ready) {
        anos_task_sleep_current_secs(1);
    }

    // Allocate page-aligned buffer for messages
    constexpr uintptr_t msg_buffer_addr = VM_TEST_BASE + 0x500000;
    const SyscallResultP map_result = anos_map_virtual(
            VM_TEST_SIZE, msg_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    if (map_result.result != SYSCALL_OK) {
        stress_test_errors++;
        message_test_complete = -1;
        anos_kill_current_task();
    }

    // Send many messages rapidly
    for (int i = 0; i < 10; i++) { // Reduced count for stress test
        char *test_message = (char *)msg_buffer_addr;
        snprintf(test_message, TEST_MESSAGE_SIZE, "Stress message #%d", i);

        const SyscallResult result =
                anos_send_message(test_channel_cookie, 0x5000 + i,
                                  strlen(test_message) + 1, test_message);
        if (result.result != SYSCALL_OK) {
            stress_test_errors++;
        }
    }

    // Clean up message buffer
    anos_unmap_virtual(VM_TEST_SIZE, msg_buffer_addr);

    message_test_complete = (stress_test_errors == 0) ? 1 : -1;
    anos_kill_current_task();
}

static int test_message_passing_stress(void) {
    SyscallResult result = anos_create_channel();
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Channel creation for stress test failed");

    test_channel_cookie = result.value;
    message_test_ready = 0;
    message_test_complete = 0;
    stress_test_errors = 0;

    // Create sender thread
    constexpr uintptr_t sender_stack = VM_TEST_BASE + 0x200000;
    const SyscallResultP map_result = anos_map_virtual(
            VM_TEST_SIZE, sender_stack,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    TEST_ASSERT(map_result.result == SYSCALL_OK,
                "Stress sender thread stack allocation failed");

    const SyscallResult thread_result = anos_create_thread(
            (ThreadFunc)stress_sender_thread, sender_stack + VM_TEST_SIZE - 8);
    TEST_ASSERT(thread_result.result == SYSCALL_OK,
                "Stress sender thread creation failed");

    message_test_ready = 1;

    // Allocate page-aligned buffer for receiving messages
    constexpr uintptr_t stress_recv_buffer_addr = VM_TEST_BASE + 0x600000;
    SyscallResultP stress_recv_map_result = anos_map_virtual(
            VM_TEST_SIZE, stress_recv_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    TEST_ASSERT(stress_recv_map_result.result == SYSCALL_OK,
                "Stress receive buffer allocation failed");

    // Receive all messages and reply to each one
    for (int i = 0; i < 10; i++) {
        char *recv_buffer = (char *)stress_recv_buffer_addr;
        uint64_t recv_tag = 0;
        size_t recv_size = TEST_MESSAGE_SIZE;

        result = anos_recv_message(test_channel_cookie, &recv_tag, &recv_size,
                                   recv_buffer);
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Stress test message receive failed");
        TEST_ASSERT(recv_tag == (0x5000 + i),
                    "Stress test received tag mismatch");

        // Reply to unblock the sender
        uint64_t message_cookie = result.value;
        SyscallResult reply_result =
                anos_reply_message(message_cookie, 0x6000 + i);
        TEST_ASSERT(reply_result.result == SYSCALL_OK,
                    "Stress test message reply failed");
    }

    // Clean up stress receive buffer
    anos_unmap_virtual(VM_TEST_SIZE, stress_recv_buffer_addr);

    // Verify sender completed successfully
    TEST_ASSERT(message_test_complete == 1,
                "Stress sender thread did not complete successfully");

    // Clean up
    anos_unmap_virtual(VM_TEST_SIZE, sender_stack);
    anos_destroy_channel(test_channel_cookie);

    return TEST_PASS;
}

// Parameter test state
static volatile int param_test_errors = 0;

static void param_receiver_thread(void) {
    while (!message_test_ready) {
        anos_task_sleep_current_secs(1);
    }

    // Allocate page-aligned buffer for receiving
    constexpr uintptr_t param_recv_buffer_addr = VM_TEST_BASE + 0x700000;
    const SyscallResultP map_result = anos_map_virtual(
            VM_TEST_SIZE, param_recv_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    if (map_result.result != SYSCALL_OK) {
        param_test_errors++;
        message_test_complete = -1;
        anos_kill_current_task();
    }

    // Test valid receive parameters first
    char *recv_buffer = (char *)param_recv_buffer_addr;
    uint64_t recv_tag = 0;
    size_t recv_size = TEST_MESSAGE_SIZE;

    SyscallResult result = anos_recv_message(test_channel_cookie, &recv_tag,
                                             &recv_size, recv_buffer);
    if (result.result != SYSCALL_OK || recv_tag != 0x9999) {
        param_test_errors++;
    } else {
        // Reply to unblock the sender
        uint64_t message_cookie = result.value;
        SyscallResult reply_result = anos_reply_message(message_cookie, 0x8888);
        if (reply_result.result != SYSCALL_OK) {
            param_test_errors++;
        }
    }

    // Test invalid receive parameters with NULL pointers - these should fail without receiving anything
    result = anos_recv_message(test_channel_cookie, NULL, &recv_size,
                               recv_buffer);
    if (result.result == SYSCALL_OK) { // Should fail
        param_test_errors++;
    }

    result = anos_recv_message(test_channel_cookie, &recv_tag, NULL,
                               recv_buffer);
    if (result.result == SYSCALL_OK) { // Should fail
        param_test_errors++;
    }

    result =
            anos_recv_message(test_channel_cookie, &recv_tag, &recv_size, NULL);
    if (result.result == SYSCALL_OK) { // Should fail
        param_test_errors++;
    }

    // Clean up
    anos_unmap_virtual(VM_TEST_SIZE, param_recv_buffer_addr);

    message_test_complete = (param_test_errors == 0) ? 1 : -1;
    anos_kill_current_task();
}

static int test_named_channel_basic(void) {
    // Create a channel
    SyscallResult result = anos_create_channel();
    TEST_ASSERT(result.result == SYSCALL_OK, "Named channel creation failed");

    const uint64_t channel_cookie = result.value;

    // Register it with a name
    char channel_name[] = "test_channel_basic";
    result = anos_register_channel_name(channel_cookie, channel_name);
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Channel name registration failed");

    // Find it by name
    result = anos_find_named_channel(channel_name);
    TEST_ASSERT(result.result == SYSCALL_OK, "Named channel lookup failed");
    TEST_ASSERT(result.value == channel_cookie,
                "Found channel cookie mismatch");

    // Deregister the name
    result = anos_remove_channel_name(channel_name);
    TEST_ASSERT(result.result == SYSCALL_OK,
                "Channel name deregistration failed");

    // Verify it can't be found anymore
    result = anos_find_named_channel(channel_name);
    TEST_ASSERT(result.value == 0,
                "Deregistered channel should not be findable");

    // Clean up
    anos_destroy_channel(channel_cookie);

    return TEST_PASS;
}

static int test_named_channel_stress(void) {
    uint64_t channels[MAX_NAMED_CHANNELS];
    char channel_names[MAX_NAMED_CHANNELS][32];

    // Create many named channels to stress the named channel registry
    for (int i = 0; i < MAX_NAMED_CHANNELS; i++) {
        SyscallResult result = anos_create_channel();
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Named channel stress creation failed");

        channels[i] = result.value;

        snprintf(channel_names[i], sizeof(channel_names[i]), "stress_ch_%d", i);

        result = anos_register_channel_name(channels[i], channel_names[i]);
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Named channel stress registration failed");
    }

    // Verify all can be found
    for (int i = 0; i < MAX_NAMED_CHANNELS; i++) {
        const SyscallResult result = anos_find_named_channel(channel_names[i]);

        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Named channel stress lookup failed");
        TEST_ASSERT(result.value == channels[i],
                    "Named channel stress cookie mismatch");
    }

    // Test name collision - try to register same name again (should fail)
    const SyscallResult collision_result =
            anos_register_channel_name(channels[0], channel_names[1]);
    TEST_ASSERT(collision_result.result != SYSCALL_OK,
                "Duplicate name registration should fail");

    // Clean up all names and channels
    for (int i = 0; i < MAX_NAMED_CHANNELS; i++) {
        SyscallResult result = anos_remove_channel_name(channel_names[i]);
        TEST_ASSERT(result.result == SYSCALL_OK,
                    "Named channel deregistration failed");

        // Verify it can't be found after deregistration
        result = anos_find_named_channel(channel_names[i]);
        TEST_ASSERT(result.value == 0,
                    "Deregistered channel should not be findable");

        anos_destroy_channel(channels[i]);
    }

    return TEST_PASS;
}

static int test_invalid_ipc_operations(void) {
    // Test operations on invalid channel with NULL buffer (should fail for multiple reasons)
    SyscallResult result = anos_send_message(0xDEADBEEF, 0, 10, NULL);
    TEST_ASSERT(result.result != SYSCALL_OK,
                "Send to invalid channel with null buffer should fail");

    // For receive test, we need page-aligned buffer even when testing invalid channel
    constexpr uintptr_t invalid_test_buffer_addr = VM_TEST_BASE + 0x800000;
    SyscallResultP invalid_map_result = anos_map_virtual(
            VM_TEST_SIZE, invalid_test_buffer_addr,
            ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_WRITE);
    TEST_ASSERT(invalid_map_result.result == SYSCALL_OK,
                "Invalid ops test buffer allocation failed");

    char *recv_buffer = (char *)invalid_test_buffer_addr;
    uint64_t recv_tag = 0;
    size_t recv_size = 64;
    result = anos_recv_message(0xDEADBEEF, &recv_tag, &recv_size, recv_buffer);
    TEST_ASSERT(result.result == SYSCALL_FAILURE,
                "Receive from invalid channel should fail");

    // Clean up invalid ops test buffer
    anos_unmap_virtual(VM_TEST_SIZE, invalid_test_buffer_addr);

    // Test invalid message parameters
    const SyscallResult channel_result = anos_create_channel();
    TEST_ASSERT(channel_result.result == SYSCALL_OK,
                "Channel creation for invalid ops test failed");

    const uint64_t channel_cookie = channel_result.value;

    // Test with null buffer (should fail due to userspace check)
    result = anos_send_message(channel_cookie, 0, 10, NULL);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Send with null buffer should fail");

    // Test named channel operations with invalid names
    result = anos_register_channel_name(channel_cookie, NULL);
    TEST_ASSERT(result.result == SYSCALL_BADARGS,
                "Register with null name should fail");

    result = anos_find_named_channel(NULL);
    TEST_ASSERT(result.result != SYSCALL_OK, "Find with null name should fail");

    // Clean up
    anos_destroy_channel(channel_cookie);

    return TEST_PASS;
}

static void run_memory_stress_tests(void) {
    anos_kprint("=== Memory Management Stress Tests ===\n");

    RUN_TEST(test_basic_virtual_mapping);
    RUN_TEST(test_virtual_mapping_stress);
    RUN_TEST(test_invalid_virtual_mappings);
    RUN_TEST(test_basic_region_management);
    RUN_TEST(test_region_coalescing);
    RUN_TEST(test_region_overlap_rejection);
    RUN_TEST(test_region_stress);
    RUN_TEST(test_invalid_regions);

    anos_kprint("=== Memory Tests Complete ===\n");
}

static void run_ipc_stress_tests(void) {
    anos_kprint("=== IPC Stress Tests ===\n");

    RUN_TEST(test_basic_channel_lifecycle);
    RUN_TEST(test_basic_message_passing);
    RUN_TEST(test_message_passing_stress);
    RUN_TEST(test_named_channel_basic);
    RUN_TEST(test_named_channel_stress);

    anos_kprint("=== IPC Tests Complete ===\n");
}

__attribute__((constructor)) void testing_init(void) {
    anos_kprint("Kernel Stress Test Server starting...\n");
}

int main(const int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        anos_kprint(argv[i]);
        anos_kprint("\n");
    }

    // Run memory management stress tests
    run_memory_stress_tests();

    // Run IPC stress tests
    run_ipc_stress_tests();

    // Print summary
    char summary[256];
    snprintf(summary, sizeof(summary),
             "\n=== TEST SUMMARY ===\nPassed: %d\nFailed: %d\nTotal: %d\n",
             test_passes, test_failures, test_passes + test_failures);
    anos_kprint(summary);

    if (test_failures == 0) {
        anos_kprint("All tests PASSED!\n");
    } else {
        anos_kprint("Some tests FAILED!\n");
    }

    // Keep the old beep/boop behavior for now
    while (1) {
        anos_task_sleep_current_secs(10);
        anos_kprint("<tests complete - beep>\n");
        anos_task_sleep_current_secs(10);
        anos_kprint("<boop>\n");
    }
}
