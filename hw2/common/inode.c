#include "inode.h"

#include <string.h>

#include "block_ops.h"

static int seek_to_inode(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t inode_id
) {
    if (inode_id == 0 || inode_id > superblock->total_inodes) {
        fprintf(stderr, "Invalid inode id\n");
        return 1;
    }

    if (fseek(file, BOOT_OFFSET + superblock->size, SEEK_SET)){
        fprintf(stderr, "Failed to seek to the beginning of the inode table\n");
        return 1;
    }
    if (fseek(file, (inode_id - 1) * INODE_SIZE, SEEK_CUR)) {
        fprintf(stderr, "Failed to seek to the inode\n");
        return 1;
    }

    return 0;
}

int write_inode(
     FILE *file,
     const struct Superblock *superblock,
     const struct Inode *inode,
     const uint32_t inode_id
) {
    if (seek_to_inode(file, superblock, inode_id))
        return 1;

    if (fwrite(inode, sizeof(*inode), 1, file) != 1) {
        fprintf(stderr, "Failed to write the inode\n");
        return 1;
    }

    return 0;
}

int read_inode (
     FILE *file,
     const struct Superblock *superblock,
     struct Inode *inode,
     const uint32_t inode_id
) {
    if (seek_to_inode(file, superblock, inode_id))
        return 1;

    if (fread(inode, sizeof(*inode), 1, file) != 1) {
        fprintf(stderr, "Failed to read the inode\n");
        return 1;
    }

    return 0;
}

static int get_indirect_block_ids(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t indirect_block_id,
     uint32_t *block_ids, // already malloc'ed
     size_t *n_block_ids
) {
    if (indirect_block_id == 0 || indirect_block_id > superblock->total_blocks) {
        fprintf(stderr, "Invalid block id\n");
        return 1;
    }

    if (read_blocks(file, superblock, &indirect_block_id, 1, (uint8_t*)block_ids, superblock->block_size)) {
        fprintf(stderr, "Failed to read indirect blocks\n");
        return 1;
    }

    *n_block_ids = 0;
    for (size_t i = 0; i < superblock->block_size / sizeof(uint32_t); ++i) {
        if (block_ids[i] != 0) {
            int block_use = get_block_use(superblock, block_ids[i]);
            if (block_use == -1) {
                fprintf(stderr, "Failed to get block use\n");
                return 1;
            }

            if (block_use)
                ++(*n_block_ids);
            else
                break;
        } else {
            break;
        }
    }

    return 0;
}

static int finalize_get_all_block_ids(
     uint32_t **block_ids,
     size_t *n_block_ids,
     const size_t offset
) {
    uint32_t *reallocated = realloc(*block_ids, offset * sizeof(uint32_t));
    if (!reallocated) {
        fprintf(stderr, "Failed to resize block_ids\n");
        free(*block_ids);
        return 1;
    }

    *block_ids = reallocated;
    *n_block_ids = offset;
    return 0;
}

int get_block_ids(
     FILE *file,
     const struct Superblock *superblock,
     const struct Inode *inode,
     uint32_t **block_ids,
     size_t *n_block_ids
) {
    const size_t indirect_len = superblock->block_size / sizeof(uint32_t);

    *block_ids = calloc(
        indirect_len * indirect_len + indirect_len + INDIRECT_BLOCK,
        sizeof(uint32_t)
    ); // maximum possible block ids
    if (!(*block_ids)) {
        fprintf(stderr, "Failed to allocate memory for block_ids\n");
        return 1;
    }

    // Direct access
    size_t offset = 0;
    for (size_t i = 0; i < INDIRECT_BLOCK; ++i) {
        if (inode->blocks[i] != 0) {
            int block_use = get_block_use(superblock, inode->blocks[i]);
            if (block_use == -1) {
                fprintf(stderr, "Failed to get block use\n");
                free(*block_ids);
                return 1;
            }

            if (block_use) {
                *(*block_ids + offset) = inode->blocks[i];
                ++offset;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    if (offset < INDIRECT_BLOCK || inode->blocks[INDIRECT_BLOCK] == 0)
        return finalize_get_all_block_ids(block_ids, n_block_ids, offset);


    // Indirect access
    size_t n_indirect_blocks;
    if (
         get_indirect_block_ids(
             file,
             superblock,
             inode->blocks[INDIRECT_BLOCK],
             *block_ids + offset,
             &n_indirect_blocks
         )
    ) {
        free(*block_ids);
        return 1;
    }

    offset += n_indirect_blocks;

    if (offset < indirect_len + INDIRECT_BLOCK || inode->blocks[DOUBLE_INDIRECT_BLOCK] == 0)
        return finalize_get_all_block_ids(block_ids, n_block_ids, offset);

    // Double indirect access
    uint32_t *double_indirect_map = malloc(superblock->block_size);
    if (!double_indirect_map) {
        fprintf(stderr, "Failed to allocate memory for double_indirect_map\n");
        free(*block_ids);
        return 1;
    }

    size_t double_indirect_map_len;
    if (
         get_indirect_block_ids(
             file,
             superblock,
             inode->blocks[DOUBLE_INDIRECT_BLOCK],
             double_indirect_map,
             &double_indirect_map_len
         )
    ) {
        free(double_indirect_map);
        free(*block_ids);
        return 1;
    }

    for (size_t i = 0; i < double_indirect_map_len; ++i) {
        if (*(double_indirect_map + i) == 0)
            break;

        size_t n_last_indirect_blocks;
        if (
             get_indirect_block_ids(
                 file,
                 superblock,
                 *(double_indirect_map + i),
                 *block_ids + offset,
                 &n_last_indirect_blocks
             )
        ) {
            free(double_indirect_map);
            free(*block_ids);
            return 1;
        }

        offset += n_last_indirect_blocks;
        if (n_last_indirect_blocks < indirect_len)
            break;
    }

    free(double_indirect_map);
    return finalize_get_all_block_ids(block_ids, n_block_ids, offset);
}

static int allocate_indirect_block(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode,
     uint32_t *where
) {
    uint32_t indirect_block_id;
    if (!get_unused_blocks(superblock, &indirect_block_id, 1))
        return 1;

    const size_t indirect_len = superblock->block_size / sizeof(uint32_t);
    uint32_t *zero_fill = calloc(indirect_len, sizeof(uint32_t));
    if (!zero_fill) {
        fprintf(stderr, "Failed to initialize memory for zero filling\n");
        return 1;
    }

    if (write_blocks(file, superblock, &indirect_block_id, 1, (const uint8_t*)zero_fill, superblock->block_size)) {
        fprintf(stderr, "Failed to zero fill the block\n");
        free(zero_fill);
        return 1;
    }

    free(zero_fill);

    if (set_block_use(superblock, indirect_block_id, 1)) {
        fprintf(stderr, "Failed to set indirect block use\n");
        return 1;
    }

    *where = indirect_block_id;
    return 0;
}

static int fill_indirect_block(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode,
     const uint32_t block_id,
     const uint32_t *data,
     const size_t n_data,
     size_t *offset
) {
    const size_t indirect_len = superblock->block_size / sizeof(uint32_t);
    uint32_t *indirect_data = calloc(indirect_len, sizeof(uint32_t));
    if (!indirect_data) {
        fprintf(stderr, "Failed to allocate memory for indirect_data\n");
        return 1;
    }

    for (size_t i = 0; i < indirect_len && *offset < n_data; ++i) {
        if (set_block_use(superblock, data[*offset], 1)) {
            fprintf(stderr, "Failed to set block use\n");
            return 1;
        }

        indirect_data[i] = data[(*offset)++];
    }

    if (write_blocks(file, superblock, &block_id, 1, (const uint8_t*)indirect_data, superblock->block_size)) {
        fprintf(stderr, "Failed to write the indirect block\n");
        free(indirect_data);
        return 1;
    }

    free(indirect_data);
    return 0;
}

int set_block_ids(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode,
     const uint32_t *block_ids,
     const size_t n_block_ids
) {
    size_t offset = 0;

    // Direct addressing
    for (size_t i = 0; i < INDIRECT_BLOCK && offset < n_block_ids; ++i) {
        if (set_block_use(superblock, block_ids[offset], 1)) {
            fprintf(stderr, "Failed to set block use\n");
            return 1;
        }

        inode->blocks[i] = block_ids[offset++];
    }

    if (offset == n_block_ids)
        return 0;

    // Indirect addressing
    if (inode->blocks[INDIRECT_BLOCK] == 0) {
        if (allocate_indirect_block(file, superblock, inode, &inode->blocks[INDIRECT_BLOCK]))
            return 1;
    } else {
        int block_use = get_block_use(superblock, inode->blocks[INDIRECT_BLOCK]);
        if (block_use == -1) {
            fprintf(stderr, "Failed to get block use\n");
            return 1;
        }

        if (!block_use) {
            if (allocate_indirect_block(file, superblock, inode, &inode->blocks[INDIRECT_BLOCK]))
                return 1;
        }
    }

    if (
         fill_indirect_block(
             file,
             superblock,
             inode,
             inode->blocks[INDIRECT_BLOCK],
             block_ids,
             n_block_ids,
             &offset
         )
    ) {
        return 1;
    }

    if (offset == n_block_ids)
        return 0;

    // Double indirect addressing
    if (inode->blocks[DOUBLE_INDIRECT_BLOCK] == 0) {
        if (allocate_indirect_block(file, superblock, inode, &inode->blocks[DOUBLE_INDIRECT_BLOCK]))
            return 1;
    } else {
        int block_use = get_block_use(superblock, inode->blocks[DOUBLE_INDIRECT_BLOCK]);
        if (block_use == -1) {
            fprintf(stderr, "Failed to get block use\n");
            return 1;
        }

        if (!block_use) {
            if (allocate_indirect_block(file, superblock, inode, &inode->blocks[DOUBLE_INDIRECT_BLOCK]))
                return 1;
        }
    }

    const size_t indirect_len = superblock->block_size / sizeof(uint32_t);
    uint32_t *double_indirect_map = calloc(indirect_len, sizeof(uint32_t));
    if (!double_indirect_map) {
        fprintf(stderr, "Failed to allocate memory for double_indirect_map\n");
        return 1;
    }

    if (
         read_blocks(
             file,
             superblock,
             &inode->blocks[DOUBLE_INDIRECT_BLOCK],
             1,
             (uint8_t*)double_indirect_map,
             superblock->block_size
         )
    ) {
        fprintf(stderr, "Failed to fill double_indirect_map\n");
        free(double_indirect_map);
        return 1;
    }

    for (size_t i = 0; i < indirect_len && offset < n_block_ids; ++i) {
        uint32_t *current_block = double_indirect_map + i;
        if (*current_block == 0) {
            if (allocate_indirect_block(file, superblock, inode, current_block)) {
                free(double_indirect_map);
                return 1;
            }
        } else {
            int block_use = get_block_use(superblock, *current_block);
            if (block_use == -1) {
                fprintf(stderr, "Failed to get block use\n");
                free(double_indirect_map);
                return 1;
            }

            if (!block_use) {
                if (allocate_indirect_block(file, superblock, inode, current_block)) {
                    free(double_indirect_map);
                    return 1;
                }
            }
        }

        if (
            fill_indirect_block(
                file,
                superblock,
                inode,
                *current_block,
                block_ids,
                n_block_ids,
                &offset
            )
        ) {
            free(double_indirect_map);
            return 1;
        }
    }

    free(double_indirect_map);
    return 0;
}

int clear_block_ids(
     FILE *file,
     struct Superblock *superblock,
     struct Inode *inode
) {
    uint32_t *used_block_ids;
    size_t n_used_block_ids;
    if (get_block_ids(file, superblock, inode, &used_block_ids, &n_used_block_ids)) {
        fprintf(stderr, "Failed to get used block ids\n");
        return 1;
    }

    for (size_t i = 0; i < n_used_block_ids; ++i) {
        if (set_block_use(superblock, used_block_ids[i], 0)) {
            fprintf(stderr, "Failed to unset block use\n");
            return 1;
        }
    }

    return 0;
}
