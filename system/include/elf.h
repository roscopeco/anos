/*
 * Elf loader for SYSTEM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

// clang-format Language: C

#ifndef __ANOS_SYSTEM_ELF_H
#define __ANOS_SYSTEM_ELF_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#define ELF_IDENT_SIZE ((16))

#define ELF_ARCH_X86_64 ((0x3e))
#define ELF_ARCH_RISCV ((0xf3))

#define PT_LOAD 1

typedef struct {
    unsigned char e_ident[ELF_IDENT_SIZE];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64Header;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64ProgramHeader;

typedef struct {
    char *filename;
    uint64_t fs_cookie;
    char *page;
    off_t current_page_offset;
} ElfPagedReader;

typedef bool (*ProgramHeaderHandler)(const ElfPagedReader *reader,
                                     const Elf64ProgramHeader *, uint64_t);

uintptr_t elf_map_elf64(ElfPagedReader *reader, ProgramHeaderHandler handler,
                        uint64_t data);

#endif