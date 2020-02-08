#include "commands.h"

#include <string.h>

#include "../common/block_ops.h"
#include "../common/constants.h"
#include "../common/directory_entry.h"
#include "../common/div_ceil.h"

int help(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    printf(
        "help           -- shows this message\n"
        "echo MESSAGE   -- prints out MESSAGE\n"
        "ls             -- lists all files in the current directory\n"
        "touch FILENAME -- creates a file called FILENAME\n"
        "mkdir DIRNAME  -- creates a directory called DIRNAME\n"
        "quit           -- quits this pseudo-shell\n"
    );

    return RETURN_SUCCESS;
}

int echo(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    if (args)
        printf("%s\n", args);
    else
        printf("\n");

    return RETURN_SUCCESS;
}

static int update(FILE *file, struct Superblock *superblock, struct FsFile *fsfile) {
    if (read_superblock(superblock, file)) {
        fprintf(stderr, "Failed to update the superblock\n");
        return 1;
    }

    if (read_inode(file, superblock, &fsfile->inode, fsfile->inode_id)) {
        fprintf(stderr, "Failed to update the inode\n");
        return 1;
    }

    return 0;
}

int ls(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    if (update(file, superblock, fsfile))
        return RETURN_ERROR;

    struct DirectoryEntry *entries;
    if (load_contents(file, superblock, fsfile, (uint8_t**)&entries)) {
        fprintf(stderr, "Failed to load directory contents\n");
        return RETURN_ERROR;
    }

    const size_t n_entries = fsfile->inode.file_size / DIRECTORY_ENTRY_SIZE;
    printf("Total %ld\n", n_entries);
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
    return RETURN_SUCCESS;
}

static int create_file(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args,
     const int filetype
) {
    if (args == NULL) {
        fprintf(stderr, "[openfs] Args cannot be NULL for this command\n");
        return RETURN_ERROR;
    }

    //if (update(file, superblock, fsfile))
    //    return RETURN_ERROR;

    const size_t name_len = strlen(args);
    if (name_len > MAX_FILENAME_LEN) {
        fprintf(stderr, "[openfs] Filename can be at most %d characters\n", MAX_FILENAME_LEN);
    }

    uint32_t inode_id;
    if (get_unused_inodes(superblock, &inode_id, 1))
        return RETURN_ERROR;

    struct DirectoryEntry entry = {
        .inode_id = inode_id,
        .filetype = filetype,
        .name_len = name_len
    };
    strncpy(entry.name, args, MAX_FILENAME_LEN - 1);

    struct DirectoryEntry *entries;
    if (load_contents(file, superblock, fsfile, (uint8_t**)&entries)) {
        fprintf(stderr, "Failed to load directory contents\n");
        return RETURN_ERROR;
    }

    for (size_t i = 0; i < fsfile->inode.file_size / DIRECTORY_ENTRY_SIZE; ++i) {
        if (strcmp(entries[i].name, entry.name) == 0) {
            fprintf(stderr, "Cannot create the file -- this filename is already used\n");
            free(entries);
            return RETURN_ERROR;
        }
    }

    const size_t new_size = fsfile->inode.file_size + DIRECTORY_ENTRY_SIZE;
    struct DirectoryEntry *temp = realloc(entries, new_size);
    if (!temp) {
        fprintf(stderr, "Failed to reallocate memory for directory entries\n");
        free(entries);
        return 1;
    }

    entries = temp;
    entries[fsfile->inode.file_size / DIRECTORY_ENTRY_SIZE] = entry;

    if (write_contents(file, superblock, fsfile, (const uint8_t*)entries, new_size)) {
        fprintf(stderr, "Failed to write the new file contents\n");
        free(entries);
        return 1;
    }

    free(entries);

    if (set_inode_use(superblock, inode_id, 1)) {
        fprintf(stderr, "Failed to set inode use\n");
        return RETURN_ERROR;
    }

    if (write_inode(file, superblock, &fsfile->inode, fsfile->inode_id)) {
        fprintf(stderr, "Failed to write the current directory inode");
        return RETURN_ERROR;
    }

    if (write_superblock(superblock, file)) {
        fprintf(stderr, "Failed to write the superblock");
        return RETURN_ERROR;
    }

    return RETURN_SUCCESS;
}

int touch(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    if (strcmp(args, "a") == 0) {
        for (int i = 1; i <= 77; ++i) {
            char str[1000] = {0};
            sprintf(str, "%d", i);
            printf("%s\n", str);

            create_file(superblock, fsfile, file, str, FILETYPE_FILE);
        }

        create_file(superblock, fsfile, file, "A", FILETYPE_FILE);
        create_file(superblock, fsfile, file, "B", FILETYPE_FILE);
        create_file(superblock, fsfile, file, "C", FILETYPE_FILE);

        return RETURN_SUCCESS;
    }

    return create_file(superblock, fsfile, file, args, FILETYPE_FILE);
}

int mkdir(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    // TODO: add creation of . and ..
    return create_file(superblock, fsfile, file, args, FILETYPE_DIRECTORY);
}

const command_func_ptr commands[] = {
    &help,
    &echo,
    &ls,
    &touch,
    &mkdir
};
const char* command_names[] = {
    "help",
    "echo",
    "ls",
    "touch",
    "mkdir"
};
const size_t n_commands = sizeof(command_names) / sizeof(const char*);
