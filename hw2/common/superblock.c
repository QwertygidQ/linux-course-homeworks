#include "superblock.h"

#include <stdlib.h>

#include "constants.h"
#include "div_ceil.h"

static size_t superblock_size(struct Superblock *superblock) {
    size_t size = 0;

    size += sizeof(superblock->magic);
    size += sizeof(superblock->total_blocks);
    size += sizeof(superblock->total_inodes);
    size += sizeof(superblock->free_blocks);
    size += sizeof(superblock->free_inodes);
    size += sizeof(superblock->block_size);

    size += sizeof(uint8_t) * superblock->used_blocks_bitmap_len;
    size += sizeof(uint8_t) * superblock->used_inodes_bitmap_len;

    return size;
}

struct Superblock create_superblock(
     const uint16_t magic,
     const uint32_t total_blocks,
     const uint32_t total_inodes,
     const uint32_t block_size
) {
    const size_t blocks_bitmap_len = DIV_CEIL(total_blocks, 8);
    const size_t inodes_bitmap_len = DIV_CEIL(total_inodes, 8);

    struct Superblock new_superblock = {
        .magic = magic,
        .total_blocks = total_blocks,
        .total_inodes = total_inodes,
        .free_blocks = total_blocks,
        .free_inodes = total_inodes,
        .block_size = block_size,
        .used_blocks_bitmap = calloc(blocks_bitmap_len, sizeof(uint8_t)),
        .used_blocks_bitmap_len = blocks_bitmap_len,
        .used_inodes_bitmap = calloc(inodes_bitmap_len, sizeof(uint8_t)),
        .used_inodes_bitmap_len = inodes_bitmap_len
    };

    new_superblock.size = superblock_size(&new_superblock);

    return new_superblock;
}

int write_superblock(const struct Superblock *superblock, FILE *file) {
    if (fseek(file, BOOT_OFFSET, SEEK_SET)) {
        fprintf(stderr, "Failed to seek to the beginning of the superblock\n");
        return 1;
    }

    if (fwrite(&superblock->magic, sizeof(superblock->magic), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's magic\n");
        return 1;
    }
    if (fwrite(&superblock->total_blocks, sizeof(superblock->total_blocks), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's total_blocks\n");
        return 1;
    }
    if (fwrite(&superblock->total_inodes, sizeof(superblock->total_inodes), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's total_inodes\n");
        return 1;
    }
    if (fwrite(&superblock->free_blocks, sizeof(superblock->free_blocks), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's free_blocks\n");
        return 1;
    }
    if (fwrite(&superblock->free_inodes, sizeof(superblock->free_inodes), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's free_inodes\n");
        return 1;
    }
    if (fwrite(&superblock->block_size, sizeof(superblock->block_size), 1, file) != 1) {
        fprintf(stderr, "Failed to write the superblock's block_size\n");
        return 1;
    }

    for (size_t i = 0; i < superblock->used_blocks_bitmap_len; ++i) {
        uint8_t bitmap_part = superblock->used_blocks_bitmap[i];
        if (fwrite(&bitmap_part, sizeof(bitmap_part), 1, file) != 1) {
            fprintf(stderr, "Failed to write the superblock's block bitmap\n");
            return 1;
        }
    }

    for (size_t i = 0; i < superblock->used_inodes_bitmap_len; ++i) {
        uint8_t bitmap_part = superblock->used_inodes_bitmap[i];
        if (fwrite(&bitmap_part, sizeof(bitmap_part), 1, file) != 1) {
            fprintf(stderr, "Failed to write the superblock's inode bitmap\n");
            return 1;
        }
    }

    return 0;
}

int read_superblock(struct Superblock *superblock, FILE *file) {
    if (fseek(file, BOOT_OFFSET, SEEK_SET)) {
        fprintf(stderr, "Failed to seek to the beginning of the superblock\n");
        return 1;
    }

    uint16_t magic;
    uint32_t total_blocks, total_inodes;
    uint32_t free_blocks, free_inodes;
    uint32_t block_size;

    if (fread(&magic, sizeof(magic), 1, file) != 1) {
        fprintf(stderr, "Failed to read the magic of the superblock\n");
        return 1;
    } else if (magic != MAGIC) {
        fprintf(stderr, "Invalid magic\n");
        return 1;
    }
    if (fread(&total_blocks, sizeof(total_blocks), 1, file) != 1) {
        fprintf(stderr, "Failed to read the total_blocks of the superblock\n");
        return 1;
    }
    if (fread(&total_inodes, sizeof(total_inodes), 1, file) != 1) {
        fprintf(stderr, "Failed to read the total_inodes of the superblock\n");
        return 1;
    }
    if (fread(&free_blocks, sizeof(free_blocks), 1, file) != 1) {
        fprintf(stderr, "Failed to read the free_blocks of the superblock\n");
        return 1;
    }
    if (fread(&free_inodes, sizeof(free_inodes), 1, file) != 1) {
        fprintf(stderr, "Failed to read the free_inodes of the superblock\n");
        return 1;
    }
    if (fread(&block_size, sizeof(block_size), 1, file) != 1) {
        fprintf(stderr, "Failed to read the block_size of the superblock\n");
        return 1;
    }

    *superblock = create_superblock(magic, total_blocks, total_inodes, block_size);
    if (superblock->used_blocks_bitmap == NULL) {
        fprintf(stderr, "Failed to allocate memory for the blocks bitmap\n");
        return 1;
    }
    if (superblock->used_inodes_bitmap == NULL) {
        fprintf(stderr, "Failed to allocate memory for the inodes bitmap\n");
        return 1;
    }

    superblock->free_blocks = free_blocks;
    superblock->free_inodes = free_inodes;

    for (size_t offset = 0; offset < superblock->used_blocks_bitmap_len; ++offset) {
        uint8_t *ptr = superblock->used_blocks_bitmap + offset;
        if (fread(ptr, sizeof(uint8_t), 1, file) != 1) {
            fprintf(stderr, "Failed to read the used_blocks_bitmap of the superblock\n");
            return 1;
        }
    }

    for (size_t offset = 0; offset < superblock->used_inodes_bitmap_len; ++offset) {
        uint8_t *ptr = superblock->used_inodes_bitmap + offset;
        if (fread(ptr, sizeof(uint8_t), 1, file) != 1) {
            fprintf(stderr, "Failed to read the used_inodes_bitmap of the superblock\n");
            return 1;
        }
    }

    return 0;
}

void free_superblock(const struct Superblock *superblock) {
    free(superblock->used_blocks_bitmap);
    free(superblock->used_inodes_bitmap);
}

int set_block_use(struct Superblock *superblock, const uint32_t block_id, const int is_used) {
    if (block_id == 0 || block_id > superblock->total_blocks) {
        fprintf(stderr, "Invalid block id\n");
        return 1;
    }

    uint8_t *bitmap_uint8 = superblock->used_blocks_bitmap + DIV_CEIL(block_id, 8) - 1;

    uint8_t mask;
    if (block_id % 8 == 0)
        mask = 1;
    else
        mask = 1 << (8 - block_id % 8);

    if (is_used)
        *bitmap_uint8 |= mask;
    else
        *bitmap_uint8 &= ~mask;

    return 0;
}

int set_inode_use(struct Superblock *superblock, const uint32_t inode_id, const int is_used) {
    if (inode_id == 0 || inode_id > superblock->total_inodes) {
        fprintf(stderr, "Invalid inode id\n");
        return 1;
    }

    uint8_t *bitmap_uint8 = superblock->used_inodes_bitmap + DIV_CEIL(inode_id, 8) - 1;

    uint8_t mask;
    if (inode_id % 8 == 0)
        mask = 1;
    else
        mask = 1 << (8 - inode_id % 8);

    if (is_used)
        *bitmap_uint8 |= mask;
    else
        *bitmap_uint8 &= ~mask;

    return 0;
}
