#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/fs_file.h"
#include "../common/directory_entry.h"
#include "../common/inode.h"
#include "../common/superblock.h"

#include "commands.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r+b");
    if (!file) {
        fprintf(stderr, "[openfs] Failed to open the file %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    struct Superblock superblock;
    if (read_superblock(&superblock, file)) {
        fprintf(stderr, "[openfs] Failed to read the superblock\n");
        return EXIT_FAILURE;
    }

    struct Inode inode;
    if (read_inode(file, &superblock, &inode, 1)) {
        fprintf(stderr, "Failed to read the root directory inode\n");
        return EXIT_FAILURE;
    }

    struct FsFile current_dir = {
        .fullname = "/",
        .inode_id = 1,
        .inode    = inode,
        .filetype = FILETYPE_DIRECTORY
    };

    int running = 1;
    while (running) {
        printf("%s > ", current_dir.fullname);

        char command[MAX_COMMAND_LEN];
        if(fgets(command, MAX_COMMAND_LEN, stdin) != command) {
            fprintf(stderr, "[openfs] Failed to read the command\n");
            continue;
        }

        const size_t newline_index = strcspn(command, "\n");
        if (newline_index < MAX_COMMAND_LEN)
            command[newline_index] = '\0';

        char *args = NULL;
        for (size_t i = 0; command[i]; ++i) {
            if (command[i] == ' ') {
                command[i] = '\0';
                args = &command[i + 1];
                break;
            } else {
                command[i] = tolower(command[i]);
            }
        }

        if (strcmp(command, "quit") == 0) {
            running = 0;
            break;
        }

        int command_recognized = 0;
        for (size_t i = 0; i < n_commands; ++i) {
            if (strcmp(command, command_names[i]) == 0) {
                int return_code = commands[i](&superblock, &current_dir, file, args);
                if (return_code == RETURN_ERROR) {
                    fprintf(stderr, "[openfs] Command returned the error code RETURN_ERROR\n");
                } else if (return_code == RETURN_CRITICAL) {
                    fprintf(stderr, "[openfs] Command returned the error code RETURN_CRITICAL\n");
                    running = 0;
                }

                command_recognized = 1;
                break;
            }
        }

        if (!command_recognized)
            fprintf(stderr, "[openfs] Unrecognized command\n");
    }

    free_superblock(&superblock);

    if (fclose(file) == EOF) {
        fprintf(stderr, "[openfs] Failed to close the file\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
