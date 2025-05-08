#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ELF_IDENT_SIZE ((16))

#define ELF_ARCH_X86_64 ((0x3e))
#define ELF_ARCH_RISCV ((0xf3))

#define VM_PAGE_SIZE ((0x1000))

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
    int fd;
    uint8_t page[VM_PAGE_SIZE];
    off_t current_page_offset;
} ElfPagedReader;

ssize_t load_page(ElfPagedReader *r, off_t offset) {
    const off_t aligned = offset & ~(VM_PAGE_SIZE - 1);
    if (aligned != r->current_page_offset) {
        if (lseek(r->fd, aligned, SEEK_SET) < 0) {
            perror("lseek");
            exit(1);
        }
        ssize_t n = read(r->fd, r->page, VM_PAGE_SIZE);
        if (n < 0) {
            perror("read");
            exit(1);
        }
        r->current_page_offset = aligned;
    }
    return VM_PAGE_SIZE;
}

static void *get_ptr(ElfPagedReader *r, const off_t offset, const size_t size) {
    static uint8_t temp[sizeof(uint64_t) * 16];
    const size_t in_page_offset = offset & (VM_PAGE_SIZE - 1);

    if (in_page_offset + size <= VM_PAGE_SIZE) {
        load_page(r, offset);
        return r->page + in_page_offset;
    }

    const size_t first_part = VM_PAGE_SIZE - in_page_offset;

    load_page(r, offset);
    memcpy(temp, r->page + in_page_offset, first_part);

    load_page(r, offset + first_part);
    memcpy(temp + first_part, r->page, size - first_part);

    return temp;
}

typedef bool (*ProgramHeaderHandler)(int, const Elf64ProgramHeader *);

static bool on_program_header(const int num, const Elf64ProgramHeader *phdr) {
    if ((phdr->p_offset & (VM_PAGE_SIZE - 1)) != 0) {
        fprintf(stderr,
                "ERROR: Segment %d file offset 0x%016llx not page aligned\n",
                num, phdr->p_offset);
        return false;
    }

    if ((phdr->p_vaddr & (VM_PAGE_SIZE - 1)) != 0) {
        fprintf(stderr, "ERROR: Segment %d vaddr 0x%16llx not page aligned\n",
                num, phdr->p_vaddr);
        return false;
    }

    printf("LOAD segment %2d: file=0x%016llx vaddr=0x%016llx filesz=0x%016llx "
           "memsz=0x%016llx\n",
           num, phdr->p_offset, phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz);

    return true;
}

bool each_elf64_program_header(ElfPagedReader *reader,
                               ProgramHeaderHandler handler) {
    const Elf64Header *ehdr = get_ptr(reader, 0, sizeof(Elf64Header));

    if (memcmp(ehdr->e_ident,
               "\x7f"
               "ELF",
               4) != 0 ||
        ehdr->e_ident[4] != 2 || ehdr->e_machine != ELF_ARCH_X86_64) {
        fprintf(stderr, "Not a valid ELF64 file\n");
        return 1;
    }

    printf("Program headers at offset: 0x%016llx\n", ehdr->e_phoff);
    printf("Program header entry size: %u bytes\n", ehdr->e_phentsize);
    printf("Number of program headers: %u\n", ehdr->e_phnum);

    printf("Entry point @ 0x%016llx\n", ehdr->e_entry);

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const off_t ph_offset = ehdr->e_phoff + i * ehdr->e_phentsize;
        const Elf64ProgramHeader *phdr =
                get_ptr(reader, ph_offset, sizeof(Elf64ProgramHeader));

        if (phdr->p_type != PT_LOAD)
            continue;

        if (!handler(i, phdr)) {
            return false;
        }
    }

    return true;
}

int main(const int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <elf-file>\n", argv[0]);
        return 1;
    }

    ElfPagedReader reader = {.fd = open(argv[1], O_RDONLY),
                             .current_page_offset = -1};

    if (reader.fd < 0) {
        perror("open");
        return 1;
    }

    each_elf64_program_header(&reader, on_program_header);

    close(reader.fd);

    return 0;
}
