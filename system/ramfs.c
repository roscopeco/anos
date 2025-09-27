/*
 * Initial ramfs for Anos
 * anos - An Operating System
 * 
 * Copyright (c)2016-2025 Ross Bamford. See LICENSE for details.
 *
 * This originally came from Mink: 
 *  https://github.com/roscopeco/mink/blob/master/ramfs.c
 *  Created on: 4 Aug 2016
 *      Author: Ross Bamford <roscopeco AT gmail DOT com>
 */

#include <stddef.h>
#include <string.h>

#include "ramfs.h"

bool is_valid_ramfs(void *mem) {
    if (mem == NULL) {
        return false;
    }

    const uint32_t magic = *((uint32_t *)mem);
    if (magic != ANOS_RAMFS_MAGIC) {
        return false;
    }

    // looks like a ramfs at this point, so lets go for the size...
    if (*((uint32_t *)mem + 1) != ANOS_RAMFS_VERSION) {
        return false;
    }

    return true;
}

int ramfs_size(AnosRAMFSHeader *fs) {
    if (!is_valid_ramfs(fs))
        return -1;

    return fs->fs_size;
}

int ramfs_file_count(AnosRAMFSHeader *fs) {
    if (!is_valid_ramfs(fs))
        return -1;

    return fs->file_count;
}

AnosRAMFSFileHeader *ramfs_find_file(AnosRAMFSHeader *fs, char *name) {
    if (!is_valid_ramfs(fs))
        return NULL;

    if (name == NULL)
        return NULL;

    // first file is at end of fs header
    AnosRAMFSFileHeader *current = (AnosRAMFSFileHeader *)(fs + 1);

    for (int i = 0; i < fs->file_count; i++, current++) {
        if (current->file_name[0] != 0 && strncmp(name, current->file_name, 24) == 0) {
            return current;
        }
    }

    return NULL;
}

void *ramfs_file_open(AnosRAMFSFileHeader *file) {
    if (file == NULL)
        return NULL;

    if (file->file_length == 0)
        return NULL;

    if (file->file_start == 0)
        return NULL;

    return (void *)(((char *)file) + file->file_start);
}