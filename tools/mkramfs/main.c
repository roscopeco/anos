/*
 * Hosted program to make a RAMFS
 * anos - An Operating System
 *
 * Copyright (c) 2025 Ross Bamford
 * 
 * This is... unpleasant...
 *   ... but it does the job.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ramfs.h"

#define FILE_COUNT(argc) (((argc - 2)))
#define COPY_BUF_SIZE 0x1000

static AnosRAMFSHeader ramfs_header;
static char zero_buf[0x1000];

// Staying away from the mess of basename / dirname...
const char *file_basename(const char *path) {
    const char *base = strrchr(path, '/');
    return (base ? base + 1 : path);
}

static int calc_fs_data_size(int argc, char **argv) {
    struct stat stat_buf;
    int files_total = 0;

    for (int i = 2; i < argc; i++) {
        int status = stat(argv[i], &stat_buf);

        if (status != 0) {
            printf("Failed to stat %s\n", argv[i]);
            return -1;
        }

        files_total += stat_buf.st_size;
    }

    return files_total;
}

static int calc_fs_size(int argc, char **argv) {
    int data_size = calc_fs_data_size(argc, argv);

    if (data_size == -1) {
        return -1;
    }

    int fs_size = data_size + (FILE_COUNT(argc) * sizeof(AnosRAMFSFileHeader)) +
                  sizeof(AnosRAMFSHeader);

    // ensure page alignment
    if (fs_size & 0xfff) {
        fs_size = (fs_size + 0x1000) & ~(0xfff);
    }
    return fs_size;
}

static void dump_fs(char *filename) {
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
        printf("mkramfs %s <output> <input1> [input2] ... [inputN]\n", argv[0]);
        return 1;
    }

    printf("Create RAMFS image %s\n", argv[1]);

    int file_count = FILE_COUNT(argc);
    int current_data_offset = 0;

    int fs_size = calc_fs_size(argc, argv);
    if (fs_size == -1) {
        printf("Failed to determine fs size\n");
        return 2;
    }

    printf("fs_size is %d\n", fs_size);

    ramfs_header.file_count = file_count;
    ramfs_header.magic = ANOS_RAMFS_MAGIC;
    ramfs_header.version = ANOS_RAMFS_VERSION;
    ramfs_header.fs_size = fs_size;

    FILE *out = fopen(argv[1], "wb");
    if (!out) {
        printf("Failed to open output file!\n");
        return 3;
    }

    if (fwrite(&ramfs_header, 1, sizeof(AnosRAMFSHeader), out) !=
        sizeof(AnosRAMFSHeader)) {
        printf("Failed to write header!\n");
        fclose(out);
        return 4;
    }

    printf("Headers occupy %ld bytes\n",
           sizeof(AnosRAMFSHeader) +
                   (sizeof(AnosRAMFSFileHeader) * file_count));

    // write file headers
    int data_area_ptr = 0;
    for (int i = 0; i < file_count; i++) {
        AnosRAMFSFileHeader file_header;
        struct stat stat_buf;

        char *input = argv[i + 2];

        int status = stat(input, &stat_buf);

        if (status != 0) {
            printf("Failed to stat %s\n", argv[i]);
            fclose(out);
            return 5;
        }

        strncpy(file_header.file_name, file_basename(input),
                ANOS_RAMFS_FILENAME_MAX);
        file_header.file_length = stat_buf.st_size;
        file_header.file_start =
                ((file_count - i) * sizeof(AnosRAMFSFileHeader)) +
                data_area_ptr;

        if (fwrite(&file_header, 1, sizeof(AnosRAMFSFileHeader), out) !=
            sizeof(AnosRAMFSFileHeader)) {
            printf("Failed to write file header for %s!\n", input);
            fclose(out);
            return 6;
        }

        printf("%s starts at %llu\n", file_header.file_name,
               file_header.file_start);

        data_area_ptr += stat_buf.st_size;
    }

    // Copy file data
    for (int i = 2; i < argc; i++) {
        FILE *in = fopen(argv[i], "rb");

        while (!feof(in)) {
            char buf[COPY_BUF_SIZE];

            int rd = fread(buf, 1, COPY_BUF_SIZE, in);
            int wr = fwrite(buf, 1, rd, out);

            if (rd != wr) {
                fclose(in);
                fclose(out);
                printf("Failed to copy buffer from %s\n", argv[i]);
                return 7;
            }
        }
    }

    int zeropad = fs_size - (sizeof(AnosRAMFSHeader) +
                             ((sizeof(AnosRAMFSFileHeader) * file_count)) +
                             data_area_ptr);

    printf("Needs %d bytes of zero pad\n", zeropad);

    int zp_wr = fwrite(zero_buf, 1, zeropad, out);
    if (zp_wr != zeropad) {
        fclose(out);
        printf("Failed to zeropad - %d bytes written", zp_wr);
        return 8;
    }

    fclose(out);

    dump_fs(argv[1]);

    return 0;
}