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
        if (*(block_ids + i) != 0)
            ++(*n_block_ids);
        else
            break;
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

int get_all_block_ids(
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
            *(*block_ids + offset) = inode->blocks[i];
            ++offset;
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

int append_block_ids(
     FILE *file,
     const struct Superblock *superblock,
     struct Inode *inode,
     const uint32_t *block_ids,
     const size_t n_block_ids
) {
    size_t offset = 0;

    int first_empty_direct = -1;
    for (size_t i = 0; i < INDIRECT_BLOCK; ++i) {
        if (inode->blocks[i] == 0) {
            first_empty_direct = i;
            break;
        }
    }

    if (first_empty_direct != -1) {
        const size_t write_len = INDIRECT_BLOCK - first_empty_direct;
        memcpy(&(inode->blocks[first_empty_direct]), block_ids + offset, write_len);
        offset += write_len;
    }

    // TODO: appending for indirect addressing

    return 0;
}
