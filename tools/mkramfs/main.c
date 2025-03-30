/*
 * Hosted program to make a RAMFS
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is... unpleasant...
 *   ... but it does the job.
 */

#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "ramfs.h"

#define PAGE_SIZE 4096

typedef struct {
    const char *path;
    const char *name;
} InputFile;

static void pad_to_alignment(FILE *out, size_t alignment) {
    long offset = ftell(out);
    size_t pad = (alignment - (offset % alignment)) % alignment;
    for (size_t i = 0; i < pad; ++i)
        fputc(0, out);
}

static void write_filesystem(const char *out_path, InputFile *files,
                             size_t file_count) {
    FILE *out = fopen(out_path, "wb");
    if (!out) {
        perror("fopen");
        exit(1);
    }

    AnosRAMFSHeader header = {.magic = ANOS_RAMFS_MAGIC,
                              .version = ANOS_RAMFS_VERSION,
                              .fs_size = 0, // to be patched
                              .file_count = file_count,
                              .reserved = {0}};

    AnosRAMFSFileHeader *file_headers =
            calloc(file_count, sizeof(AnosRAMFSFileHeader));
    if (!file_headers) {
        perror("calloc");
        exit(1);
    }

    // Header + all file headers padded to page boundary
    size_t headers_raw =
            sizeof(header) + file_count * sizeof(AnosRAMFSFileHeader);
    size_t headers_padded = (headers_raw + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Write zero-filled space for header and headers
    fwrite(&header, sizeof(header), 1, out);
    fwrite(file_headers, sizeof(AnosRAMFSFileHeader), file_count, out);
    for (size_t i = headers_raw; i < headers_padded; ++i)
        fputc(0, out);

    size_t current_offset = headers_padded;

    for (size_t i = 0; i < file_count; ++i) {
        FILE *f = fopen(files[i].path, "rb");
        if (!f) {
            perror(files[i].path);
            exit(1);
        }

        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        file_headers[i].file_start =
                current_offset -
                (sizeof(AnosRAMFSHeader) + i * sizeof(AnosRAMFSFileHeader));
        file_headers[i].file_length = file_size;
        strncpy(file_headers[i].file_name, files[i].name,
                sizeof(file_headers[i].file_name) - 1);

        // Copy file data
        char buf[4096];
        size_t remaining = file_size;
        while (remaining > 0) {
            size_t read = fread(buf, 1, sizeof(buf), f);
            fwrite(buf, 1, read, out);
            remaining -= read;
        }

        fclose(f);

        current_offset += file_size;
        size_t pad = (PAGE_SIZE - (current_offset % PAGE_SIZE)) % PAGE_SIZE;
        for (size_t j = 0; j < pad; ++j)
            fputc(0, out);
        current_offset += pad;
    }

    // Patch file headers
    fseek(out, sizeof(header), SEEK_SET);
    fwrite(file_headers, sizeof(AnosRAMFSFileHeader), file_count, out);

    // Patch fs_size
    fseek(out, 0, SEEK_END);
    uint64_t final_size = (ftell(out) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    fseek(out, offsetof(AnosRAMFSHeader, fs_size), SEEK_SET);
    fwrite(&final_size, sizeof(final_size), 1, out);

    fclose(out);
    free(file_headers);
}

static void dump_fs(const char *filename) {
    struct stat stat_buf;

    int status = stat(filename, &stat_buf);

    if (status != 0) {
        printf("dump_fs: Failed to stat %s\n", filename);
        return;
    }

    FILE *fs = fopen(filename, "rb");
    if (!fs) {
        printf("dump_fs: Failed to open %s\n", filename);
        return;
    }

    AnosRAMFSHeader *buf = malloc(stat_buf.st_size);

    if (!buf) {
        printf("dump_fs: Failed to allocate %lld bytes for fs\n",
               stat_buf.st_size);
        fclose(fs);
        return;
    }

    int rd = fread(buf, 1, stat_buf.st_size, fs);
    fclose(fs);

    if (rd != stat_buf.st_size) {
        printf("dump_fs: Failed to read FS from %s\n", filename);
        free(buf);
        return;
    }

    AnosRAMFSFileHeader *hdr = (AnosRAMFSFileHeader *)(buf + 1);

    for (int i = 0; i < buf->file_count; i++) {
        if (!hdr) {
            printf("dump_fs: Could not find file %s\n", hdr->file_name);
            free(buf);
            return;
        }

        uint8_t *fbuf = ramfs_file_open(hdr);

        if (!fbuf) {
            printf("dump_fs: Could not open file %s\n", hdr->file_name);
            free(buf);
            return;
        }

        printf("%-20s [%10lld]: ", hdr->file_name, hdr->file_length);
        for (int i = 0; i < 16; i++) {
            printf("0x%02x ", *fbuf++);
        }
        printf(" ... \n");

        hdr++;
    }

    printf("dump_fs success\n");
    free(buf);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output.img> <input1> [input2] ...\n",
                argv[0]);
        return 1;
    }

    const char *out_path = argv[1];
    size_t file_count = argc - 2;

    InputFile *files = malloc(file_count * sizeof(InputFile));
    if (!files) {
        perror("malloc");
        return 1;
    }

    for (size_t i = 0; i < file_count; ++i) {
        const char *path = argv[i + 2];
        char *path_copy = strdup(path);
        if (!path_copy) {
            perror("strdup");
            return 1;
        }

        const char *base = basename(path_copy);
        if (strlen(base) >= 16) {
            fprintf(stderr,
                    "Filename '%s' too long for AnosRAMFS (max 15 chars)\n",
                    base);
            return 1;
        }

        files[i].path = path;
        files[i].name = strdup(base); // hold onto a clean copy

        free(path_copy);
    }

    write_filesystem(out_path, files, file_count);

    for (size_t i = 0; i < file_count; ++i) {
        free((void *)files[i].name);
    }
    free(files);

    dump_fs(out_path);
    return 0;
}
