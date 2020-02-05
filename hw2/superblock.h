#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdlib.h>
#include <stdint.h>

struct Superblock {
    uint16_t magic;
    uint32_t total_blocks, total_inodes;
    uint32_t free_blocks, free_inodes;
    uint32_t block_size, inode_size;
};

const size_t SUPERBLOCK_SIZE = sizeof(struct Superblock);

#endif
