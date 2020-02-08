#ifndef FS_FILE_H
#define FS_FILE_H

#include <stdint.h>
#include <stdio.h>

#include "inode.h"
#include "superblock.h"

struct FsFile {
    char         *fullname;
    uint32_t     inode_id;
    struct Inode inode;
};

int load_contents(
    FILE *file,
    const struct Superblock *superblock,
    const struct FsFile *fsfile,
    uint8_t **ptr
);
int write_contents(
    FILE *file,
    struct Superblock *superblock,
    struct FsFile *fsfile,
    const uint8_t *ptr,
    const size_t ptr_size
);

#endif
