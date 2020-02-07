#include "commands.h"

#include <string.h>

#include "../common/block_ops.h"
#include "../common/constants.h"
#include "../common/directory_entry.h"
#include "../common/div_ceil.h"

int help(
     struct Superblock *superblock,
     struct Inode* current_dir,
     uint32_t *dir_inode_id,
     FILE *file,
     char* args
) {
    printf(
        "help -- shows this message\n"
        "echo MESSAGE -- prints out MESSAGE\n"
        "ls -- lists all files in the current directory\n"
        "touch FILENAME -- creates a file called FILENAME\n"
    );

    return RETURN_SUCCESS;
}

int echo(
     struct Superblock *superblock,
     struct Inode* current_dir,
     uint32_t *dir_inode_id,
     FILE *file,
     char* args
) {
    if (args)
        printf("%s\n", args);
    else
        printf("\n");

    return RETURN_SUCCESS;
}

int ls(
     struct Superblock *superblock,
     struct Inode* current_dir,
     uint32_t *dir_inode_id,
     FILE *file,
     char* args
) {
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

int touch(
     struct Superblock *superblock,
     struct Inode* current_dir,
     uint32_t *dir_inode_id,
     FILE *file,
     char* args
) {
    if (args == NULL) {
        fprintf(stderr, "[openfs] Args cannot be NULL for this command\n");
        return RETURN_ERROR;
    }

    const size_t name_len = strlen(args);
    if (name_len > MAX_FILENAME_LEN) {
        fprintf(stderr, "[openfs] Filename can be at most %d characters\n", MAX_FILENAME_LEN);
    }

    uint32_t inode_id;
    if (get_unused_inodes(superblock, &inode_id, 1))
        return RETURN_ERROR;

    struct DirectoryEntry entry = {
        .inode_id = inode_id,
        .filetype = FILETYPE_FILE,
        .name_len = name_len
    };
    strncpy(entry.name, args, MAX_FILENAME_LEN);

    const size_t entry_blocks = DIV_CEIL(DIRECTORY_ENTRY_SIZE, superblock->block_size);
    uint32_t *block_ids = calloc(entry_blocks, sizeof(uint32_t));
    if (!block_ids) {
        fprintf(stderr, "Failed to allocate the memory for the block_ids\n");
        return RETURN_ERROR;
    }

    if (get_unused_blocks(superblock, block_ids, entry_blocks)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    if (append_block_ids(file, superblock, current_dir, block_ids, entry_blocks)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    if (set_inode_use(superblock, inode_id, 1)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    for (size_t i = 0; i < entry_blocks; ++i) {
        if (set_block_use(superblock, *(block_ids + i), 1)) {
            free(block_ids);
            return RETURN_ERROR;
        }
    }

    if (write_inode(file, superblock, current_dir, *dir_inode_id)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    if (write_blocks(file, superblock, block_ids, entry_blocks, &entry, DIRECTORY_ENTRY_SIZE)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    if (write_superblock(superblock, file)) {
        free(block_ids);
        return RETURN_ERROR;
    }

    free(block_ids);
    return RETURN_SUCCESS;
}

const command_func_ptr commands[] = {
    &help,
    &echo,
    &ls,
    &touch
};
const char* command_names[] = {
    "help",
    "echo",
    "ls",
    "touch"
};
const size_t n_commands = sizeof(command_names) / sizeof(const char*);
