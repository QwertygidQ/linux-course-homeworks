#ifndef DIRECTORY_OPS_H
#define DIRECTORY_OPS_H

#include <stdio.h>

#include "fs_file.h"
#include "superblock.h"

int find_file(
    FILE *file,
    const struct Superblock *superblock,
    const struct FsFile *where,
    const char *filename,
    struct FsFile *found_handle
);

#endif
