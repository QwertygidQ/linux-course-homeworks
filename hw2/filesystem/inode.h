#ifndef INODE_H
#define INODE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "constants.h"
#include "superblock.h"

struct Inode {
    uint32_t file_size;
    uint32_t links_count;
    uint32_t blocks[INODE_BLOCK_COUNT];
};

int write_inode(
    FILE *file,
    const struct Superblock *superblock,
    const struct Inode *inode,
    const uint32_t inode_id
);

int read_inode (
    FILE *file,
    const struct Superblock *superblock,
    struct Inode *inode,
    const uint32_t inode_id
);

int clear_inode(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t inode_id
);

int get_block_ids(
    FILE *file,
    const struct Superblock *superblock,
    const struct Inode *inode,
    uint32_t **block_ids,
    size_t *n_block_ids
);

int set_block_ids(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode,
     const uint32_t *block_ids,
     const size_t n_block_ids
);

int clear_block_ids(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode
);

#define INODE_SIZE sizeof(struct Inode)

#endif
