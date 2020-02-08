#include "block_ops.h"

#include <string.h>

#include "constants.h"

static int seek_to_block(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t block_id
) {
    if (block_id == 0 || block_id > superblock->total_blocks) {
        fprintf(stderr, "Invalid block id\n");
        return 1;
    }

    if (
        fseek(
            file,
            BOOT_OFFSET +
             superblock->size +
             (block_id - 1) * superblock->block_size,
            SEEK_SET
        )
    ) {
        fprintf(stderr, "Failed to seek to the block\n");
        return 1;
    }

    return 0;
}

int read_blocks(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t *block_ids,
     const size_t n_block_ids,
     uint8_t *ptr,
     const size_t ptr_size
) {
    for (size_t i = 0; i < n_block_ids; ++i) {
        const uint32_t current_block_id = *(block_ids + i);
        if (seek_to_block(file, superblock, current_block_id))
            return 1;

        const size_t offset = i * superblock->block_size;
        if (offset >= ptr_size) {
            fprintf(stderr, "ptr is too small\n");
            return 1;
        } else if (ptr_size - offset < superblock->block_size) {
            const size_t block_part_size = ptr_size - offset;
            if (fread(ptr + offset, block_part_size, 1, file) != 1) {
                fprintf(stderr, "Failed to read the final block part\n");
                return 1;
            }
        } else {
            if (fread(ptr + offset, superblock->block_size, 1, file) != 1) {
                fprintf(stderr, "Failed to read a block\n");
                return 1;
            }
        }
    }

    return 0;
}

int write_blocks(
     FILE *file,
     const struct Superblock *superblock,
     const uint32_t *block_ids,
     const size_t n_block_ids,
     const uint8_t *ptr,
     const size_t ptr_size
) {
    for (size_t i = 0; i < n_block_ids; ++i) {
        const uint32_t current_block_id = *(block_ids + i);
        if (seek_to_block(file, superblock, current_block_id))
            return 1;

        const size_t offset = i * superblock->block_size;
        if (offset >= ptr_size) {
            fprintf(stderr, "Not enough data in the ptr, offset too big\n");
            return 1;
        } else if (ptr_size - offset < superblock->block_size) {
            const size_t block_part_size = ptr_size - offset;
            if (fwrite(ptr + offset, block_part_size, 1, file) != 1) {
                fprintf(stderr, "Failed to write the final block part\n");
                return 1;
            }
            for (size_t j = 0; j < superblock->block_size - block_part_size; ++j)
                fputc(0, file);
        } else {
            if (fwrite(ptr + offset, superblock->block_size, 1, file) != 1) {
                fprintf(stderr, "Failed to write a block\n");
                return 1;
            }
        }
    }

    return 0;
}
