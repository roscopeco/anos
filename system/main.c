/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include <stdint.h>
#include <stdnoreturn.h>

#include <anos.h>
#include <anos/printf.h>
#include <anos/syscalls.h>
#include <anos/types.h>

#include "ramfs.h"

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

extern AnosRAMFSHeader _system_ramfs_start;

volatile int num;
static uint64_t channel_cookie;
static uint64_t channel2_cookie;

int subroutine(int in) { return in * 2; }

static uint8_t t2_stack[4096];
static uint8_t t3_stack[4096];
static uint8_t t4_stack[4096];

static inline void banner() {
    printf("\n\nSYSTEM User-mode Supervisor #%s [libanos #%s]\n", VERSION,
           libanos_version());
}

static void thread2() {
    int count = 0;
    printf("\nTask 2 - startup and send messages to channel 2 (no receivers "
           "there yet, T3 will listen when it wakes)...\n");
    uint64_t r1 = anos_send_message(channel2_cookie, 0x1b33b, 0x1b00b);
    printf("\nTask 2 recieved reply 1 : %lx\n", r1);
    uint64_t r2 = anos_send_message(channel2_cookie, 0x2b33b, 0x2b00b);
    printf("\nTask 2 recieved reply 2 : %lx - will sleep for 5 secs\n", r2);

    anos_task_sleep_current_secs(5);

    printf("\nTask 2 has arisen!\n");

    uint64_t tag, arg;
    uint64_t recvd = anos_recv_message(channel_cookie, &tag, &arg);
    printf("T2: Received message on channel 1: cookie 0x%016lx: [tag = "
           "0x%08lx; arg = 0x%08lx]\n",
           recvd, tag, arg);

    anos_reply_message(recvd, 192000);

    while (1) {
        if (count++ % 100000000 == 0) {
            anos_kputchar('2');
        }
    }
}

static void thread3() {
    int count = 0;
    printf("Task 3 - startup and immediately sleep...\n");
    anos_task_sleep_current_secs(3);
    printf("\nTask 3 has arisen!\n");

    uint64_t tag, arg;

    while (1) {
        uint64_t recvd = anos_recv_message(channel2_cookie, &tag, &arg);

        printf("T3: Received message on channel 2: cookie 0x%016lx: [tag = "
               "0x%08lx; arg = 0x%08lx]\n",
               recvd, tag, arg);

        anos_reply_message(recvd, count++);
    }
}

static void thread4() {
    printf("Task 4 - startup and immediately sleep...\n");
    anos_task_sleep_current_secs(10);
    printf("\nTask 4 has arisen - Send message to channel 1!\n");

    uint64_t response = anos_send_message(channel_cookie, 9001, 2);
    printf("T4: Response is %ld; going to sleep-print loop...\n", response);

    while (1) {
        anos_task_sleep_current_secs(4);
        anos_kputchar('4');
    }
}

static noreturn int other_main(void) {
    anos_kprint("Beep Boop process is up...\n");

    while (1) {
        anos_task_sleep_current_secs(10);
        anos_kprint("<beep>");
        anos_task_sleep_current_secs(10);
        anos_kprint("<boop>");
    }

    __builtin_unreachable();
}

noreturn int initial_server_loader(void);

#ifdef DEBUG_INIT_RAMFS
static void dump_fs(AnosRAMFSHeader *ramfs) {
    AnosRAMFSFileHeader *hdr = (AnosRAMFSFileHeader *)(ramfs + 1);

    printf("System FS magic: 0x%08x\n", ramfs->magic);
    printf("System FS ver  : 0x%08x\n", ramfs->version);
    printf("System FS count: 0x%016lx\n", ramfs->file_count);
    printf("System FS size : 0x%016lx\n", ramfs->fs_size);

    for (int i = 0; i < ramfs->file_count; i++) {
        if (!hdr) {
            printf("dump_fs: Could not find file %s\n", hdr->file_name);
            return;
        }

        uint8_t *fbuf = ramfs_file_open(hdr);

        if (!fbuf) {
            printf("dump_fs: Could not open file %s\n", hdr->file_name);
            return;
        }

        printf("%-20s [%10ld]: ", hdr->file_name, hdr->file_length);
        for (int i = 0; i < 16; i++) {
            printf("0x%02x ", *fbuf++);
        }
        printf(" ... \n");

        hdr++;
    }
}
#else
#define dump_fs(...)
#endif

int main(int argc, char **argv) {
    banner();

    AnosMemInfo meminfo;
    if (anos_get_mem_info(&meminfo) == 0) {
        printf("%ld / %ld bytes used / free\n",
               meminfo.physical_total - meminfo.physical_avail,
               meminfo.physical_avail);
    } else {
        printf("WARN: Get mem info failed\n");
    }

    channel_cookie = anos_create_channel();
    printf("Created IPC channel #1 0x%016lx\n", channel_cookie);

    channel2_cookie = anos_create_channel();
    printf("Created IPC channel #2 0x%016lx\n", channel2_cookie);

    if (anos_register_channel_name(channel_cookie, "SYSTEM::DBG:CHANNEL") !=
        0) {
        printf("Failed to name channel #1!\n");
    } else {
        printf("Named channel 1...\n");
    }

    channel_cookie = anos_find_named_channel("SYSTEM::DBG:CHANNEL");
    if (!channel_cookie) {
        printf("Failed to retrieve named channel #1!\n");
    } else {
        printf("Retrieved cookie 0x%016lx by name '%s'\n", channel_cookie,
               "SYSTEM::DBG:CHANNEL");
    }

#ifdef DEBUG_TEST_SYSCALL
    uint64_t ret;
    if ((ret = anos_testcall_int(1, 2, 3, 4, 5)) == 42) {
        anos_kprint("GOOD\n");
    } else {
        anos_kprint("BAD\n");
    }

    if ((ret = anos_testcall_syscall(1, 2, 3, 4, 5)) == 42) {
        anos_kprint("GOOD\n");
    } else {
        anos_kprint("BAD\n");
    }
#endif

#ifdef DEBUG_INIT_RAMFS
    AnosRAMFSHeader *ramfs = (AnosRAMFSHeader *)&_system_ramfs_start;
    dump_fs(ramfs);
#endif

    int t2 = anos_create_thread(thread2, ((uintptr_t)t2_stack) + 0x1000);
    if (t2 == 0) {
        printf("Thread 2 creation failed!\n");
    } else {
        printf("Thread 2 created with tid 0x%02x\n", t2);
    }

    int t3 = anos_create_thread(thread3, ((uintptr_t)t3_stack) + 0x1000);
    if (t3 == 0) {
        printf("Thread 3 creation failed!\n");
    } else {
        printf("Thread 3 created with tid 0x%02x\n", t3);
    }

    int t4 = anos_create_thread(thread4, ((uintptr_t)t4_stack) + 0x1000);
    if (t4 == 0) {
        printf("Thread 4 creation failed!\n");
    } else {
        printf("Thread 4 created with tid 0x%02x\n", t4);
    }

    ProcessMemoryRegion regions[2] = {
            {
                    .start = 0x1000000,
                    .len_bytes = 0x3000,
            },
            {.start = 0x80000000, .len_bytes = 0x4000}};

    uint64_t new_pid = anos_create_process(0x7ffff000, 0x1000, 2, regions,
                                           (uintptr_t)other_main);
    if (new_pid < 0) {
        printf("Failed to create new process\n");
    } else {
        printf("Created new process with PID 0x%016lx\n", new_pid);
    }

    num = 1;
    int count = 0;

    while (1) {
        num = subroutine(num);

        if (count++ % 100000000 == 0) {
            anos_kputchar('1');
        }
    }
}