#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/constants.h"
#include "../common/directory_entry.h"
#include "../common/div_ceil.h"
#include "../common/inode.h"
#include "../common/superblock.h"
#include "../common/write_ops.h"

#include "defaults.h"

void print_usage(const char *launch_name) {
    fprintf(
        stderr,
        "Usage: %s "
        "FILE "
        "[BLOCK_SIZE "
        "TOTAL_BLOCKS "
        "TOTAL_INODES]\n",
        launch_name
    );
}

int parse_arguments(int argc, char *argv[], struct Superblock *superblock, char **file) {
    uint32_t block_size   = DEFAULT_BLOCK_SIZE;
    uint32_t total_blocks = DEFAULT_TOTAL_BLOCKS;
    uint32_t total_inodes = DEFAULT_TOTAL_INODES;

    if (argc != 2 && argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 5) {
        uint32_t* args[] = {
            &block_size,
            &total_blocks,
            &total_inodes
        };
        for (size_t i = 2; i < argc; ++i) {
            char *end;
            errno = 0;
            long next = strtol(argv[i], &end, 10);
            if (end == argv[i]   ||
                    *end != '\0'    ||
                    errno == ERANGE ||
                    next < 0        ||
                    next > UINT32_MAX) {
                print_usage(argv[0]);
                return 1;
            }

            *args[i - 2] = next;
        }
    }

    *file = argv[1];
    *superblock = create_superblock(MAGIC, total_blocks, total_inodes, block_size);
    if (superblock->used_blocks_bitmap == NULL) {
        fprintf(stderr, "Failed to allocate memory for the blocks bitmap\n");
        return 1;
    }
    if (superblock->used_inodes_bitmap == NULL) {
        fprintf(stderr, "Failed to allocate memory for the inodes bitmap\n");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *filename;
    struct Superblock superblock;
    if (parse_arguments(argc, argv, &superblock, &filename))
        return EXIT_FAILURE;

    printf("[mkfs] Resulting superblock size: %d\n", superblock.size);
    printf("[mkfs] Creating a file system in %s\n", filename);
    printf("[mkfs] BLOCK_SIZE: %d\n", superblock.block_size);
    printf("[mkfs] TOTAL_BLOCKS: %d\n", superblock.total_blocks);
    printf("[mkfs] TOTAL_INODES: %d\n", superblock.total_inodes);

    // Opening the file
    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "[mkfs] Failed to open the file %s\n", filename);
        free_superblock(&superblock);
        return EXIT_FAILURE;
    }

    // Padding the file
    const size_t filesize = BOOT_OFFSET + superblock.size + superblock.total_blocks * superblock.block_size;
    if (fseek(file, filesize  - 1, SEEK_SET) || fputc(0, file) == EOF) {
        fprintf(stderr, "[mkfs] Failed to pad the file to the correct size\n");
        free_superblock(&superblock);
        return EXIT_FAILURE;
    }

    printf("[mkfs] Padded the file\n");

    // Reserving blocks for the inode table
    const size_t inode_table_size = INODE_SIZE * superblock.total_inodes;
    const size_t inode_table_blocks = DIV_CEIL(inode_table_size, superblock.block_size);

    superblock.free_blocks -= inode_table_blocks;
    for (size_t i = 1; i <= inode_table_blocks; ++i) {
        if (set_block_use(&superblock, i, 1))
            return EXIT_FAILURE;
    }

    printf("[mkfs] Reserved %d blocks for the inode table\n", inode_table_blocks);

    // Writing the dot directory entry for the root directory
    struct DirectoryEntry root_dot = {
        .inode_id = 1,
        .filetype = FILETYPE_DIRECTORY,
        .name_len = 1,
        .name     = "."
    };

    const size_t n_root_dot_blocks = DIV_CEIL(DIRECTORY_ENTRY_SIZE, superblock.block_size);
    uint32_t *block_ids = calloc(sizeof(uint32_t), n_root_dot_blocks);
    if (block_ids == NULL) {
        fprintf(stderr, "[mkfs] Failed to allocate memory for block_ids\n");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n_root_dot_blocks; ++i) {
        const uint32_t block_id = inode_table_blocks + 1 + i;
        block_ids[i] = block_id;

        if (set_block_use(&superblock, block_id, 1))
            return EXIT_FAILURE;
    }
    superblock.free_blocks -= n_root_dot_blocks;

    if (write_blocks(file, &superblock, block_ids, n_root_dot_blocks, &root_dot, DIRECTORY_ENTRY_SIZE))
        return EXIT_FAILURE;

    free(block_ids);

    printf("[mkfs] Wrote the root directory dot entry (%d blocks)\n", n_root_dot_blocks);

    // Writing the inode for the root directory
    uint32_t root_blocks[INODE_BLOCK_COUNT];
    for (size_t i = 0; i < n_root_dot_blocks; ++i) {
        const uint32_t block_id = inode_table_blocks + 1 + i;
        root_blocks[i] = block_id;
    }
    for (size_t i = n_root_dot_blocks; i < INODE_BLOCK_COUNT; ++i)
        root_blocks[i] = 0;

    struct Inode root = {
        .file_size   = DIRECTORY_ENTRY_SIZE,
        .links_count = 1
    };
    memcpy(root.blocks, root_blocks, sizeof(root.blocks));

    if(write_inode(file, &superblock, &root, 1))
        return EXIT_FAILURE;

    --superblock.free_inodes;
    if(set_inode_use(&superblock, 1, 1))
        return EXIT_FAILURE;

    printf("[mkfs] Wrote the root directory inode\n");

    // Writing the superblock
    if (write_superblock(&superblock, file)) {
        fprintf(stderr, "[mkfs] Failed to write the superblock\n");
        free_superblock(&superblock);
        return EXIT_FAILURE;
    }

    // Closing the file
    if (fclose(file) == EOF) {
        fprintf(stderr, "[mkfs] Failed to close the file\n");
        free_superblock(&superblock);
        return EXIT_FAILURE;
    }

    printf("[mkfs] Success!\n");
    printf(
        "[mkfs] Free inodes: %d/%d, free blocks: %d/%d\n",
        superblock.free_inodes,
        superblock.total_inodes,
        superblock.free_blocks,
        superblock.total_blocks
    );

    free_superblock(&superblock);
    return EXIT_SUCCESS;
}
