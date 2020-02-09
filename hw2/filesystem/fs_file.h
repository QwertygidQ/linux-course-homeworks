#ifndef FS_FILE_H
#define FS_FILE_H

#include <stdint.h>
#include <stdio.h>

#include "directory_entry.h"
#include "inode.h"
#include "superblock.h"

struct FsFile {
    struct Inode inode;
    uint32_t     inode_id;
    uint8_t      filetype;
    char         *fullname;
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

int clear_file(
    FILE *file,
    struct Superblock *superblock,
    struct FsFile *fsfile
);

int remove_file(
     FILE *file,
     struct Superblock *superblock,
     struct FsFile *directory,
     const char *filename
);

#endif
