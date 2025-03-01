/*
 * Initial ramfs for Anos
 * anos - An Operating System
 * 
 * Copyright (c)2016-2025 Ross Bamford. See LICENSE for details.
 *
 * The initial ramfs is a small filesystem loaded along with SYSTEM.
 * It holds the basic subset of drivers and servers required to initiate
 * the rest of the boot process.
 *
 * As it stands, it's a stretch to even call this a filesystem. It's basically
 * just a block of memory that has named blocks within it (i.e the files).
 * When a file is "opened", you literally just get back a pointer to the
 * start of that files data - it's up to you to make sure you don't
 * read past it, corrupt it, or whatever...
 *
 * System just maps this (readonly) at the root of the VFS and uses it as
 * a place from which to load the initial servers - the driver manager,
 * and initial essential drivers (disk etc).
 *
 * This originally came from Mink: 
 *  https://github.com/roscopeco/mink/blob/master/include/ramfs.h
 *  Created on: 4 Aug 2016
 *      Author: Ross Bamford <roscopeco AT gmail DOT com>
 */

#ifndef __ANOS_SYSTEM_RAMFS_H_
#define __ANOS_SYSTEM_RAMFS_H_

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#define ANOS_RAMFS_MAGIC 0x0101CA75
#define ANOS_RAMFS_VERSION 10
#define ANOS_RAMFS_FILENAME_MAX 15

typedef struct {
    uint32_t magic;      /* 0x0101CA75 */
    uint32_t version;    /* Must equal ANOS_RAMFS_VERSION */
    uint64_t fs_size;    /* Size of this filesystem. Must be a multiple of 4k */
    uint64_t file_count; /* Number of AnosRAMFSFileHeader following header */
} __attribute__((packed)) AnosRAMFSHeader;

typedef struct {
    uint64_t file_start;  /* offset from this header to start of data */
    uint64_t file_length; /* size of file, in bytes. */
    char file_name[16];   /* File-name, null-terminated. */
} __attribute__((packed)) AnosRAMFSFileHeader;

/*
  * Checks that the memory pointed to by mem is a valid ramfs.
  * This includes a magic check and a version check.
  */
bool is_valid_ramfs(void *mem);

/*
  * Gets the size of the ramfs, as reported by the header.
  * Returns -1 if the fs is not valid.
  */
int ramfs_size(AnosRAMFSHeader *fs);

/*
  * Gets the number of files in the ramfs, as reported by the header.
  * Returns -1 if the fs is not valid.
  */
int ramfs_file_count(AnosRAMFSHeader *fs);

/*
  * Finds the named file from the given fs.
  * Returns NULL if the fs is not valid, or if the name is NULL.
  */
AnosRAMFSFileHeader *ramfs_find_file(AnosRAMFSHeader *fs, char *name);

/*
  * 'Open' the given file. Just returns a pointer to the start of the data.
  * Returns NULL if the given file is NULL or empty.
  */
void *ramfs_file_open(AnosRAMFSFileHeader *file);

#endif /* __ANOS_SYSTEM_RAMFS_H_ */