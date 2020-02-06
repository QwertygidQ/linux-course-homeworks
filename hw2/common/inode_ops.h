#ifndef INODE_OPS_H
#define INODE_OPS_H

#include <stdio.h>
#include <stdlib.h>

#include "superblock.h"
#include "inode.h"

int write_inode(
    FILE *file,
    const struct Superblock *superblock,
    const struct Inode *inode,
    const size_t inode_id
);

int read_inode (
    FILE *file,
    const struct Superblock *superblock,
    struct Inode *inode,
    const size_t inode_id
);

#endif
