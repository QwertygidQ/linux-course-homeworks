#include "commands.h"

#include "../common/block_ops.h"
#include "../common/directory_entry.h"

int help(struct Superblock *superblock, struct Inode* current_dir, FILE *file, char* args) {
    printf(
        "help -- shows this message\n"
        "ls -- lists all files in the current directory\n"
        "echo MESSAGE -- prints out MESSAGE\n"
    );

    return RETURN_SUCCESS;
}

int echo(struct Superblock *superblock, struct Inode* current_dir, FILE *file, char* args) {
    if (args)
        printf("%s\n", args);
    else
        printf("\n");

    return RETURN_SUCCESS;
}

int ls(struct Superblock *superblock, struct Inode* current_dir, FILE *file, char* args) {
    uint32_t *block_ids;
    size_t n_block_ids;
    if (get_all_block_ids(file, superblock, current_dir, &block_ids, &n_block_ids)) {
        fprintf(stderr, "[openfs] Failed to read the directory blocks\n");
        return RETURN_ERROR;
    }

    struct DirectoryEntry *entries = malloc(current_dir->file_size);
    if (read_blocks(file, superblock, block_ids, n_block_ids, entries, current_dir->file_size)) {
        fprintf(stderr, "[openfs] Failed to read the directory contents\n");
        free(entries);
        free(block_ids);
        return RETURN_ERROR;
    }

    const size_t n_entries = current_dir->file_size / DIRECTORY_ENTRY_SIZE;
    printf("Total %d\n", n_entries);
    for (size_t i = 0; i < n_entries; ++i) {
        char *filetype_str;
        switch (entries[i].filetype) {
        case FILETYPE_DIRECTORY:
            filetype_str = "DIR";
            break;
        case FILETYPE_FILE:
            filetype_str = "FILE";
            break;
        default:
            filetype_str = "???";
            break;
        }

        printf("%d\t%s\t%s\n", entries[i].inode_id, filetype_str, entries[i].name);
    }

    free(entries);
    free(block_ids);
    return RETURN_SUCCESS;
}

const command_func_ptr commands[] = {
    &help,
    &echo,
    &ls
};
const char* command_names[] = {
    "help",
    "echo",
    "ls"
};
const size_t n_commands = sizeof(command_names) / sizeof(const char*);
