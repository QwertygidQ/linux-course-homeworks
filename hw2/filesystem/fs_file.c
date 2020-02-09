#include "fs_file.h"

#include <stdlib.h>

#include "constants.h"
#include "block_ops.h"
#include "div_ceil.h"

int load_contents(
     FILE *file,
     const struct Superblock *superblock,
     const struct FsFile *fsfile,
     uint8_t **ptr
) {
    uint32_t *block_ids;
    size_t n_block_ids;
    if (get_block_ids(file, superblock, &fsfile->inode, &block_ids, &n_block_ids)) {
        fprintf(stderr, "Failed to get block ids in load_contents()\n");
        return 1;
    }

    *ptr = malloc(fsfile->inode.file_size);
    if (!*ptr) {
        fprintf(stderr, "Failed to allocate memory for ptr in load_contents()\n");
        free(block_ids);
        return 1;
    }

    if (read_blocks(file, superblock, block_ids, n_block_ids, *ptr, fsfile->inode.file_size)) {
        fprintf(stderr, "Failed to read the file's blocks\n");
        free(block_ids);
        free(*ptr);
        return 1;
    }

    free(block_ids);
    return 0;
}

int write_contents(
     FILE *file,
     struct Superblock *superblock,
     struct FsFile *fsfile,
     const uint8_t *ptr,
     const size_t ptr_size
) {
    if (clear_block_ids(file, superblock, &fsfile->inode)) {
        fprintf(stderr, "Failed to clear block ids\n");
        return 1;
    }

    const size_t ptr_blocks = DIV_CEIL(ptr_size * sizeof(uint8_t), superblock->block_size);
    uint32_t *block_ids = calloc(ptr_blocks, superblock->block_size);
    if (!block_ids) {
        fprintf(stderr, "Failed to allocate memory for block_ids\n");
        return 1;
    }

    if (get_unused_blocks(superblock, block_ids, ptr_blocks)) {
        fprintf(stderr, "Failed to get unused blocks\n");
        free(block_ids);
        return 1;
    }

    if (set_block_ids(file, superblock, &fsfile->inode, block_ids, ptr_blocks)) {
        fprintf(stderr, "Failed to set block ids\n");
        free(block_ids);
        return 1;
    }

    if (write_blocks(file, superblock, block_ids, ptr_blocks, ptr, ptr_size)) {
        fprintf(stderr, "Failed to write file contents\n");
        free(block_ids);
        return 1;
    }

    fsfile->inode.file_size = ptr_size;

    free(block_ids);
    return 0;
}

int clear_file(
     FILE *file,
     struct Superblock *superblock,
     struct FsFile *fsfile
) {
    uint8_t *zeroed = calloc(fsfile->inode.file_size / sizeof(uint8_t), sizeof(uint8_t));
    if (!zeroed) {
        fprintf(stderr, "Failed to initialize zeroed memory\n");
        return 1;
    }

    if (write_contents(file, superblock, fsfile, zeroed, fsfile->inode.file_size)) {
        fprintf(stderr, "Failed to zero out memory\n");
        free(zeroed);
        return 1;
    }

    free(zeroed);

    if (clear_block_ids(file, superblock, &fsfile->inode)) {
        fprintf(stderr, "Failed to clear block ids\n");
        return 1;
    }

    if (clear_inode(file, superblock, fsfile->inode_id)) {
        fprintf(stderr, "Failed to clear inode\n");
        return 1;
    }

    return 0;
}
