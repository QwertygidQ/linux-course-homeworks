#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/constants.h"
#include "../common/div_ceil.h"
#include "../common/inode.h"
#include "../common/inode_ops.h"
#include "../common/superblock.h"

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

    if (argc != 2 && argc != 6) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc == 6) {
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

    return 0;
}

int main(int argc, char *argv[]) {
    char *filename;
    struct Superblock superblock;
    if (parse_arguments(argc, argv, &superblock, &filename))
        return EXIT_FAILURE;

    printf("[mkfs] Resulting superblock size: %d\n", superblock.size);
    printf("[mkfs] Creating a file system in %s\n", filename);

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

    // Writing the inode for the root directory
    const size_t inode_table_size = INODE_SIZE * superblock.total_inodes;
    const size_t inode_table_blocks = DIV_CEIL(inode_table_size, superblock.block_size);

    uint32_t root_blocks[INODE_BLOCK_COUNT];
    root_blocks[0] = inode_table_blocks + 1;
    for (size_t i = 1; i < INODE_BLOCK_COUNT; ++i)
        root_blocks[i] = 0;

    struct Inode root = {
        .file_size   = 0,
        .links_count = 1
    };
    memcpy(root.blocks, root_blocks, sizeof(root.blocks));

    write_inode(file, &superblock, &root, 1);

    // Setting the inode and block bitmaps for the root inode and the inode table
    superblock.free_blocks -= inode_table_blocks + 1;
    for (size_t block = 1; block <= inode_table_blocks + 1; ++block)
        set_block_use(&superblock, block, 1);

    --superblock.free_inodes;
    set_inode_use(&superblock, 1, 1);

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

    free_superblock(&superblock);
    return EXIT_SUCCESS;
}
