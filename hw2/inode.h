#ifndef INODE_H
#define INODE_H

#include <stdint.h>

#include "constants.h"

struct Inode {
    uint32_t file_size;
    uint32_t links_count;
    uint32_t blocks[INODE_BLOCK_COUNT];
};

#define INODE_SIZE sizeof(struct Inode)

#endif
