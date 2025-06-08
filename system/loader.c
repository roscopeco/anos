/*
 * Bootstrap process loader
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This serves as the entrypoint for a new process loaded
 * from the RAMFS by SYSTEM.
 * 
 * NOTE: This is a bit weird, because although this is SYSTEM
 * code, it actually runs in the context of the new process.
 * 
 * So for the first bit of every new process, SYSTEM's code 
 * and data/bss etc are mapped in, and the process has SYSTEM's
 * capabilities so it can load the binary and map static 
 * memory etc.
 * 
 * The code here is responsible for removing those mappings 
 * before handing control over to the user code.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <string.h>

#ifndef UNIT_TESTS
#include <anos/syscalls.h>
#else
uint64_t anos_find_named_channel(const char *name);
uint64_t anos_send_message(uint64_t cookie, uint64_t tag, int len, void *msg);
char *anos_map_virtual(uint64_t size, uint64_t addr);
void anos_kprint(const char *msg);
void anos_task_sleep_current_secs(int secs);
#endif

#include "elf.h"
#include "loader.h"
#include "printf.h"

#ifdef DEBUG_SERVER_LOADER
#define debugf printf
#else
#define debugf(...)
#endif

typedef void (*ServerEntrypoint)(void);

const extern void *_code_start;
const extern void *_code_end;
const extern void *_bss_start;
const extern void *_bss_end;

// TODO we shouldn't have inline x86_64 in here!!!
static noreturn void restore_stack_and_jump(void *stack_ptr,
                                            void (*target)(void)) {
#ifdef __x86_64__
    __asm__ volatile("mov %0, %%rsp\n"
                     "jmp *%1\n"
                     :
                     : "r"(stack_ptr), "r"(target));
#elifdef __riscv
    __asm__ volatile("mv sp, %0\n"
                     "jr %1\n"
                     :
                     : "r"(stack_ptr), "r"(target));
#else
#error Need an arch-specific restore_stack_and_jump in system:loader.c
#endif
    __builtin_unreachable();
}

static bool on_program_header(const ElfPagedReader *reader,
                              const Elf64ProgramHeader *phdr,
                              const uint64_t ramfs_cookie) {
    if ((phdr->p_offset & (VM_PAGE_SIZE - 1)) != 0) {
        debugf("ERROR: %s: Segment file offset 0x%016lx not page aligned\n",
               reader->filename, phdr->p_offset);
        return false;
    }

    if ((phdr->p_vaddr & (VM_PAGE_SIZE - 1)) != 0) {
        debugf("ERROR: %s Segment vaddr 0x%16lx not page aligned\n",
               reader->filename, phdr->p_vaddr);
        return false;
    }

    debugf("LOAD: %s: segment file=0x%016lx vaddr=0x%016lx filesz=0x%016lx "
           "memsz=0x%016lx\n",
           reader->filename, phdr->p_offset, phdr->p_vaddr, phdr->p_filesz,
           phdr->p_memsz);

    // TODO support flags in syscall for RO / RW / NX etc...
    char *buffer = anos_map_virtual(phdr->p_memsz, phdr->p_vaddr,
                                    ANOS_MAP_VIRTUAL_FLAG_READ |
                                            ANOS_MAP_VIRTUAL_FLAG_WRITE |
                                            ANOS_MAP_VIRTUAL_FLAG_EXEC);

    if (!buffer) {
        return false;
    }

    memset(buffer, 0, phdr->p_memsz);

    if (phdr->p_filesz > 0) {
        for (int i = 0; i < phdr->p_filesz; i += 0x1000) {
            char *msg_buffer = (char *)(phdr->p_vaddr + i);
            uint64_t *const pos = (uint64_t *)(phdr->p_vaddr + i);
            *pos = i + phdr->p_offset;

            strncpy(msg_buffer + sizeof(uint64_t), reader->filename, 1024);

            const int loaded_bytes = anos_send_message(
                    ramfs_cookie, SYS_VFS_TAG_LOAD_PAGE, 26, msg_buffer);

            if (!loaded_bytes) {
                anos_kprint("FAILED TO LOAD: 0 bytes\n");
                return false;
            }
        }
    }

    return true;
}

static void unmap_system_memory() {
#ifndef UNIT_TESTS
    // NOTE keep this in-step with setup in main.c!
    // const uintptr_t code_start = (uintptr_t)&_code_start;
    // const uintptr_t code_end = (uintptr_t)&_code_end;
    // const size_t code_len = code_end - code_start;
    const uintptr_t bss_start = (uintptr_t)&_bss_start;
    const uintptr_t bss_end = (uintptr_t)&_bss_end;
    const size_t bss_len = bss_end - bss_start;

    // TODO this isn't actually going to work this way, we're still running
    //      code in this mapping, unmapping it like this won't end well...
    //
    // anos_unmap_virtual(code_len, code_start);

    anos_unmap_virtual(bss_len, bss_start);
#endif
}

noreturn void initial_server_loader_bounce(void *initial_sp, char *filename) {
    printf("\nLoading '%s'...\n", filename);

    const uint64_t sys_vfs_cookie = anos_find_named_channel("SYSTEM::VFS");

    if (!sys_vfs_cookie) {
        printf("Failed to find named VFS channel\n");
        anos_kill_current_task();
    }

    char *msg_buffer = anos_map_virtual(0x1000, 0x1fff000,
                                        ANOS_MAP_VIRTUAL_FLAG_READ |
                                                ANOS_MAP_VIRTUAL_FLAG_WRITE);

    strncpy(msg_buffer, filename, 1024);

    const uint64_t sys_ramfs_cookie =
            anos_send_message(sys_vfs_cookie, 1, 22, msg_buffer);

    if (!sys_ramfs_cookie) {
        anos_kprint("Failed to find RAMFS driver. Dying.\n");
        anos_kill_current_task();
    }

    const uint64_t exec_size = anos_send_message(
            sys_ramfs_cookie, SYS_VFS_TAG_GET_SIZE, 22, msg_buffer);

    if (exec_size) {
        ElfPagedReader reader = {.current_page_offset = -1,
                                 .fs_cookie = sys_ramfs_cookie,
                                 .page = msg_buffer,
                                 .filename = filename};

        const uintptr_t entrypoint =
                elf_map_elf64(&reader, on_program_header, sys_ramfs_cookie);
        if (entrypoint) {
            const ServerEntrypoint sep = (ServerEntrypoint)(entrypoint);

            unmap_system_memory();

            restore_stack_and_jump(initial_sp, sep);
        }

        anos_kprint("Unable to load executable\n");
    } else {
        anos_kprint("Unable to get size\n");
    }

    anos_kprint("Server exec failed. Dying.\n");
    anos_kill_current_task();
}