#include "directory_ops.h"

#include <string.h>

#include "directory_entry.h"
#include "inode.h"
#include "misc.h"

int find_file(
    FILE *file,
    const struct Superblock *superblock,
    const struct FsFile *where,
    const char *filename,
    struct FsFile *found_handle
) {
    struct FsFile current;
    char *split_filename;

    if (filename[0] == '/') {
        current = (struct FsFile){
            .fullname = "/",
            .inode_id = 1,
            .filetype = FILETYPE_DIRECTORY
        };
        if (read_inode(file, superblock, &current.inode, 1)) {
            fprintf(stderr, "Failed to read the root directory inode\n");
            return 1;
        }

        if (strcmp(filename, "/") == 0) {
            *found_handle = current;
            return 0;
        }

        split_filename = malloc(strlen(filename) - 1);
        if (!split_filename) {
            fprintf(stderr, "Failed to allocate memory for split_filename\n");
            return 1;
        }
        strcpy(split_filename, filename + 1);
    } else {
        current = *where;
        split_filename = malloc(strlen(filename));
        if (!split_filename) {
            fprintf(stderr, "Failed to allocate memory for split_filename\n");
            return 1;
        }
        strcpy(split_filename, filename);
    }

    char *next = strtok(split_filename, "/");
    do {
        if (current.filetype != FILETYPE_DIRECTORY) {
            fprintf(stderr, "Is not a directory\n");
            return 1;
        }

        struct DirectoryEntry *entries;
        if (load_contents(file, superblock, &current, (uint8_t**)&entries)) {
            fprintf(stderr, "Failed to load current directory contents\n");
            free(split_filename);
            return 1;
        }

        int found = 0;
        for (size_t i = 0; i < current.inode.file_size / DIRECTORY_ENTRY_SIZE; i++) {
            if (strcmp(entries[i].name, next) == 0) {
                char *new_path;
                if (path_join(current.fullname, next, &new_path)) {
                    free(entries);
                    free(split_filename);
                    return 1;
                }
                //free(current->fullname);

                current = (struct FsFile){
                    .fullname = new_path,
                    .inode_id = entries[i].inode_id,
                    .filetype = entries[i].filetype
                };
                if (read_inode(file, superblock, &current.inode, current.inode_id)) {
                    fprintf(stderr, "Failed to read inode\n");
                    free(entries);
                    free(split_filename);
                    return 1;
                }

                found = 1;
                break;
            }
        }

        free(entries);

        if (!found) {
            fprintf(stderr, "File not found\n");
            free(split_filename);
            return 1;
        }
    } while (next = strtok(NULL, "/"));

    free(split_filename);
    *found_handle = current;
    return 0;
}
