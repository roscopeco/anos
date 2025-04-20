/*
 * stage3 - Tests for IPC channel
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "munit.h"

#include "mock_recursive.h"
#include "mock_vmm.h"

#include "ipc/channel.h"
#include "ipc/channel_internal.h"
#include "structs/hash.h"
#include "structs/list.h"

/* Dummy panic that aborts the test on failure */
void panic_sloc(const char *msg, const char *filename, const uint64_t line) {
    munit_errorf("%s", msg);
}

/* Dummy implementation of kernel_guard_once */
void kernel_guard_once(void) { /* no-op for tests */ }

/* --- CPU timer --- */
uint64_t cpu_read_tsc(void) {
    /* Return a fixed value; note that ipc_channel_create
        will adjust the cookie based on this value */
    return 1000;
}

/* --- SpinLock API --- */
void spinlock_init(SpinLock *lock) { (void)lock; }
void spinlock_lock(SpinLock *lock) { (void)lock; }
void spinlock_unlock(SpinLock *lock) { (void)lock; }

/* --- Slab Allocator Mocks --- */
void *slab_alloc_block(void) {
    /* For testing we simply allocate 64 bytes */
    return malloc(64);
}
void slab_free(void *ptr) { free(ptr); }

/* --- List API --- */

/* Simple list_add: add node at end */
ListNode *list_add(ListNode *head, ListNode *node) {
    while (head->next)
        head = head->next;
    head->next = node;
    node->next = NULL;

    return node;
}

/* --- Scheduler and Task Mocks --- */
/* We simulate task_current() via a global pointer */
static Task *current_task_ptr = NULL;
Task *task_current(void) { return current_task_ptr; }

/* Dummy tasks for simulation */
static Task sender_task;
static Task receiver_task;

/* Dummy PerCPUState and related scheduler functions */
typedef struct PerCPUState {
    int dummy;
} PerCPUState;

PerCPUState *sched_find_target_cpu(void) {
    static PerCPUState cpu;
    return &cpu;
}
uint64_t sched_lock_any_cpu(PerCPUState *cpu) {
    (void)cpu;
    return 0;
}
void sched_unlock_any_cpu(PerCPUState *cpu, uint64_t flags) {
    (void)cpu;
    (void)flags;
}

/* In these mocks the block/unblock/schedule functions are no-ops */
void sched_block(Task *task) { (void)task; }
void sched_unblock(Task *task) { (void)task; }
void sched_unblock_on(Task *task, PerCPUState *cpu) {
    (void)task;
    (void)cpu;
}
void sched_schedule(void) { /* no-op */ }
uint64_t sched_lock_this_cpu(void) { return 0; }
void sched_unlock_this_cpu(uint64_t flags) { (void)flags; }

/* --- End Mocks and Stubs --- */

/* Extern declarations for globals used in the IPC channel module */
extern HashTable *channel_hash;
extern HashTable *in_flight_message_hash;

static void *test_setup(const MunitParameter params[], void *user_data) {
    ipc_channel_init();

    current_task_ptr = NULL;
    return NULL;
}

/* --- Test Cases --- */

/* Test that a channel can be created and then destroyed */
static MunitResult test_channel_create_destroy(const MunitParameter params[],
                                               void *data) {
    uint64_t channel_cookie = ipc_channel_create();
    munit_assert_int(channel_cookie, !=, 0);

    /* Verify that the channel exists in the hash table */
    void *ch = hash_table_lookup(channel_hash, channel_cookie);
    munit_assert_not_null(ch);

    ipc_channel_destroy(channel_cookie);

    /* After destruction the channel should no longer be found */
    ch = hash_table_lookup(channel_hash, channel_cookie);
    munit_assert_null(ch);

    return MUNIT_OK;
}

// This needs to be page-aligned, and gcc (13, Linux) won't do that
// if it's on the stack apparently...
uint64_t __attribute__((aligned(0x1000))) buf;

/* Test ipc_channel_recv when a message is already queued.
    We manually allocate an IpcMessage and insert it into the channel queue. */
static MunitResult test_recv_with_queued_message(const MunitParameter params[],
                                                 void *data) {

    uint64_t channel_cookie = ipc_channel_create();
    munit_assert_int(channel_cookie, !=, 0);

    IpcChannel *channel = hash_table_lookup(channel_hash, channel_cookie);
    munit_assert_not_null(channel);

    /* Allocate and set up a fake message */
    IpcMessage *msg = slab_alloc_block();
    munit_assert_not_null(msg);
    msg->this.next = NULL;
    msg->cookie = 12345; /* Test cookie */
    msg->tag = 42;
    msg->arg_buf_phys = 0x1000; // must be page-aligned
    msg->arg_buf_size = 99;
    msg->waiter = task_current();
    msg->reply = 0;
    msg->handled = false;

    /* Manually insert the message into the channel's queue */
    spinlock_lock(channel->queue_lock);
    channel->queue = msg;
    spinlock_unlock(channel->queue_lock);

    uint64_t tag;
    size_t size;

    uint64_t ret_cookie = ipc_channel_recv(channel_cookie, &tag, &size, &buf);

    // Values set as in message
    munit_assert_int(ret_cookie, ==, 12345);
    munit_assert_int(tag, ==, 42);
    munit_assert_int(size, ==, 99);

    // Buffer correctly mapped...
    munit_assert_uint64(mock_vmm_get_last_page_map_paddr(), ==, 0x1000);
    munit_assert_uint64(mock_vmm_get_last_page_map_vaddr(), ==, (uint64_t)&buf);

    /* Verify that the message is now in the in-flight message hash table */
    IpcMessage *lookup_msg = hash_table_lookup(in_flight_message_hash, 12345);
    munit_assert_not_null(lookup_msg);

    /* Clean up: remove the message from the hash table */
    hash_table_remove(in_flight_message_hash, 12345);
    ipc_channel_destroy(channel_cookie);
    slab_free(msg);

    return MUNIT_OK;
}

/* Test that replying to a message works correctly.
    We manually insert a message into the in-flight message hash table, call reply,
    and verify that the reply is set and the message is removed. */
static MunitResult test_reply(const MunitParameter params[], void *data) {

    IpcMessage *msg = slab_alloc_block();
    munit_assert_not_null(msg);
    msg->this.next = NULL;
    msg->cookie = 54321;
    msg->tag = 0;
    msg->arg_buf_size = 0;
    msg->waiter = task_current();
    msg->reply = 0;
    msg->handled = false;

    /* Insert the message into the in-flight message hash table */
    hash_table_insert(in_flight_message_hash, msg->cookie, msg);

    uint64_t ret = ipc_channel_reply(msg->cookie, 999);
    munit_assert_int(ret, ==, 54321);
    munit_assert_int(msg->reply, ==, 999);

    /* Verify the message has been removed from the hash table */
    IpcMessage *lookup_msg =
            hash_table_lookup(in_flight_message_hash, msg->cookie);
    munit_assert_null(lookup_msg);

    slab_free(msg);
    return MUNIT_OK;
}

/* Test that sending on an invalid (non-existent) channel returns 0 */
static MunitResult test_send_invalid_channel(const MunitParameter params[],
                                             void *data) {

    uint64_t ret = ipc_channel_send(99999, 1, 2,
                                    (void *)3); /* Use an invalid cookie */
    munit_assert_int(ret, ==, 0);
    return MUNIT_OK;
}

/* Test that receiving on an invalid channel returns 0 */
static MunitResult test_recv_invalid_channel(const MunitParameter params[],
                                             void *data) {

    uint64_t tag, buf;
    size_t size;
    uint64_t ret =
            ipc_channel_recv(99999, &tag, &size, &buf); /* Invalid channel */
    munit_assert_int(ret, ==, 0);
    return MUNIT_OK;
}

/* Test sending when a receiver is waiting.
    Here we manually add a waiting receiver to the channel and then
    call ipc_channel_send. The sender will notice the waiting receiver,
    unblock it, and eventually return (with no reply set, so 0 is returned). */
static MunitResult
test_send_when_receiver_waiting(const MunitParameter params[], void *data) {
    uint64_t channel_cookie = ipc_channel_create();
    munit_assert_int(channel_cookie, !=, 0);
    IpcChannel *channel = hash_table_lookup(channel_hash, channel_cookie);
    munit_assert_not_null(channel);

    /* Set a waiting receiver */
    channel->receivers = &receiver_task;
    receiver_task.this.next = NULL;

    /* Set the current task to the sender */
    current_task_ptr = &sender_task;

    uint64_t ret = ipc_channel_send(channel_cookie, 10, 20, (void *)0x1000);

    /* No reply was provided so the sender should receive 0 */
    munit_assert_int(ret, ==, 0);
    munit_assert_null(channel->receivers);

    ipc_channel_destroy(channel_cookie);
    return MUNIT_OK;
}

/* --- Test Suite Registration --- */
static MunitTest test_suite_tests[] = {
        {"/create_destroy", test_channel_create_destroy, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/recv_with_queued_message", test_recv_with_queued_message, test_setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/reply", test_reply, test_setup, NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {"/send_invalid_channel", test_send_invalid_channel, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/recv_invalid_channel", test_recv_invalid_channel, test_setup, NULL,
         MUNIT_TEST_OPTION_NONE, NULL},
        {"/send_receiver_waiting", test_send_when_receiver_waiting, test_setup,
         NULL, MUNIT_TEST_OPTION_NONE, NULL},
        {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}};

static const MunitSuite test_suite = {"/ipc/channel", test_suite_tests,
                                      NULL, /* no suite-level setup */
                                      1,    /* iterations */
                                      MUNIT_SUITE_OPTION_NONE};

int main(int argc, char *argv[]) {
    return munit_suite_main(&test_suite, NULL, argc, argv);
}
