#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct Superblock {
    uint16_t magic;
    uint32_t total_blocks, total_inodes;
    uint32_t free_blocks, free_inodes;
    uint32_t block_size;
    uint8_t  *used_blocks_bitmap;
    size_t   used_blocks_bitmap_len;
    uint8_t  *used_inodes_bitmap;
    size_t   used_inodes_bitmap_len;
    size_t   size;
};

struct Superblock create_superblock(
    const uint16_t magic,
    const uint32_t total_blocks,
    const uint32_t total_inodes,
    const uint32_t block_size
);

int  write_superblock(const struct Superblock *superblock, FILE *file);
int  read_superblock (struct Superblock *superblock, FILE *file);
void free_superblock (const struct Superblock *superblock);

int set_block_use(struct Superblock *superblock, const uint32_t block_id, const int is_used);
int set_inode_use(struct Superblock *superblock, const uint32_t inode_id, const int is_used);

int get_block_use(const struct Superblock *superblock, const uint32_t block_id);
int get_inode_use(const struct Superblock *superblock, const uint32_t inode_id);

int get_unused_blocks(const struct Superblock *superblock, uint32_t *blocks, const size_t n_blocks);
int get_unused_inodes(const struct Superblock *superblock, uint32_t *inodes, const size_t n_inodes);

#endif
