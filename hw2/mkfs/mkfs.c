#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "../constants.h"
#include "../defaults.h"
#include "../inode.h"
#include "../superblock.h"

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
        return 0;
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
                return 0;
            }

            *args[i - 2] = next;
        }
    }

    *file = argv[1];
    *superblock = (struct Superblock){
        .magic        = DEFAULT_MAGIC,
        .total_blocks = total_blocks,
        .total_inodes = total_inodes,
        .free_blocks  = total_blocks,
        .free_inodes  = total_inodes,
        .block_size   = block_size,
        .inode_size   = INODE_SIZE
    };

    return 1;
}

int main(int argc, char *argv[]) {
    char *filename;
    struct Superblock superblock;
    if (!parse_arguments(argc, argv, &superblock, &filename))
        return EXIT_FAILURE;

    printf("[mkfs] Creating a file system in %s\n", filename);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "[mkfs] Failed to open the file %s\n", filename);
        return EXIT_FAILURE;
    }

    if (fseek(file, BOOT_OFFSET, SEEK_SET)) {
        fprintf(stderr, "[mkfs] Failed to seek to the beginning of the superblock\n");
        return EXIT_FAILURE;
    }

    if (fwrite(&superblock, SUPERBLOCK_SIZE, 1, file) != 1) {
        fprintf(stderr, "[mkfs] Failed to write the superblock\n");
        return EXIT_FAILURE;
    }

    if (fseek(file, superblock.total_blocks * superblock.block_size - 1, SEEK_CUR) ||
         fputc(0, file) == EOF) {
        fprintf(stderr, "[mkfs] Failed to pad the file to the correct size\n");
        return EXIT_FAILURE;
    }

    if (fclose(file) == EOF) {
        fprintf(stderr, "[mkfs] Failed to close the file\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
