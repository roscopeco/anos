/*
* Elf loader for SYSTEM
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 */

#include <anos/syscalls.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "elf.h"

#define VM_PAGE_SIZE ((0x1000))

#define SYS_VFS_TAG_GET_SIZE ((0x1))
#define SYS_VFS_TAG_LOAD_PAGE ((0x2))

#ifdef DEBUG_SERVER_LOADER
#include "printf.h"
#define debugf printf
#else
#define debugf(...)
#endif

ssize_t load_page(ElfPagedReader *r, const off_t offset) {
    const off_t aligned = offset & ~(VM_PAGE_SIZE - 1);
    if (aligned != r->current_page_offset) {
        debugf("Loading page at 0x%016lx\n", offset);
        uint64_t *const pos = (uint64_t *)r->page;
        *pos = offset;

        strcpy(r->page + sizeof(uint64_t), r->filename);

        const int loaded_bytes = anos_send_message(
                r->fs_cookie, SYS_VFS_TAG_LOAD_PAGE, 26, r->page);

        if (!loaded_bytes) {
            anos_kprint("FAILED TO LOAD: 0 bytes\n");
            return 0;
        }

        r->current_page_offset = aligned;
    }

    return VM_PAGE_SIZE;
}

static void *get_ptr(ElfPagedReader *r, const off_t offset, const size_t size) {
    static uint8_t temp[sizeof(uint64_t) * 16];
    const size_t in_page_offset = offset & (VM_PAGE_SIZE - 1);

    if (in_page_offset + size <= VM_PAGE_SIZE) {
        if (!load_page(r, offset)) {
            return NULL;
        }
        return r->page + in_page_offset;
    }

    const size_t first_part = VM_PAGE_SIZE - in_page_offset;

    if (!load_page(r, offset)) {
        return NULL;
    }
    memcpy(temp, r->page + in_page_offset, first_part);

    if (!load_page(r, offset + first_part)) {
        return NULL;
    }
    memcpy(temp + first_part, r->page, size - first_part);

    return temp;
}

uintptr_t elf_map_elf64(ElfPagedReader *reader,
                        const ProgramHeaderHandler handler, uint64_t data) {

    const Elf64Header *ehdr = get_ptr(reader, 0, sizeof(Elf64Header));

    if (!ehdr) {
        debugf("Failed to read header\n");
        return 0;
    }

    const uintptr_t entry = ehdr->e_entry;

    if (memcmp(ehdr->e_ident,
               "\x7f"
               "ELF",
               4) != 0 ||
        ehdr->e_ident[4] != 2) {
        debugf("Not a valid ELF64 file\n");
        return 0;
    }

    debugf("Program headers at offset: 0x%016lx\n", ehdr->e_phoff);
    debugf("Program header entry size: %u bytes\n", ehdr->e_phentsize);
    debugf("Number of program headers: %u\n", ehdr->e_phnum);

    debugf("Entry point @ 0x%016lx\n", ehdr->e_entry);

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const off_t ph_offset = ehdr->e_phoff + i * ehdr->e_phentsize;
        const Elf64ProgramHeader *phdr =
                get_ptr(reader, ph_offset, sizeof(Elf64ProgramHeader));

        if (phdr->p_type != PT_LOAD)
            continue;

        if (!handler(reader, phdr, data)) {
            return 0;
        }
    }

    return entry;
}
