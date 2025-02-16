/*
 * Entry point for the user-mode system manager
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

#include "anos.h"
#include "anos/anos_syscalls.h"
#include "anos/anos_types.h"
#include "anos/printf.h"
#include <stdint.h>

#ifndef VERSTR
#warning Version String not defined (-DVERSTR); Using default
#define VERSTR #unknown
#endif

#define XSTRVER(verstr) #verstr
#define STRVER(xstrver) XSTRVER(xstrver)
#define VERSION STRVER(VERSTR)

volatile int num;

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
    printf("\nTask 2 - startup and immediately sleep...\n");
    anos_task_sleep_current_secs(5);
    printf("\nTask 2 has arisen!\n");

    while (1) {
        if (count++ % 100000000 == 0) {
            kputchar('2');
        }
    }
}

static void thread3() {
    int count = 0;
    printf("Task 3 - startup and immediately sleep...\n");
    anos_task_sleep_current_secs(3);
    printf("\nTask 3 has arisen!\n");

    while (1) {
        if (count++ % 100000000 == 0) {
            kputchar('3');
        }
    }
}

static void thread4() {
    int count = 0;
    printf("Task 4 - startup and immediately sleep...\n");
    anos_task_sleep_current_secs(10);
    printf("\nTask 4 has arisen!\n");

    while (1) {
        if (count++ % 100000000 == 0) {
            kputchar('4');
            anos_task_sleep_current_secs(2);
        }
    }
}

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

#ifdef DEBUG_TEST_SYSCALL
    uint64_t ret;
    if ((ret = testcall_int(1, 2, 3, 4, 5)) == 42) {
        kprint("GOOD\n");
    } else {
        kprint("BAD\n");
    }

    if ((ret = testcall_syscall(1, 2, 3, 4, 5)) == 42) {
        kprint("GOOD\n");
    } else {
        kprint("BAD\n");
    }
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

    if (anos_create_process(0x1000000, 0x3000) < 0) {
        printf("FAILED TO CREATE PROCESS\n");
    }

    num = 1;
    int count = 0;

    while (1) {
        num = subroutine(num);

        if (count++ % 100000000 == 0) {
            kputchar('1');
        }
    }
}