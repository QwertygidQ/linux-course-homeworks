#include "commands.h"

#include <string.h>

#include "../common/block_ops.h"
#include "../common/constants.h"
#include "../common/directory_entry.h"
#include "../common/directory_ops.h"
#include "../common/div_ceil.h"
#include "../common/misc.h"

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
        "cd DIRNAME     -- changes your current directory to DIRNAME\n"
        "stat FILE      -- get information about FILE\n"
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
    struct DirectoryEntry *entries;
    if (load_contents(file, superblock, fsfile, (uint8_t**)&entries)) {
        fprintf(stderr, "Failed to load directory contents\n");
        return RETURN_ERROR;
    }

    const size_t n_entries = fsfile->inode.file_size / DIRECTORY_ENTRY_SIZE;
    printf("Total %ld\n", n_entries);
    for (size_t i = 0; i < n_entries; ++i)
        printf("%d\t%s\t%s\n", entries[i].inode_id, filetype_str(entries[i].filetype), entries[i].name);

    free(entries);
    return RETURN_SUCCESS;
}

static int create_file(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args,
     const uint8_t filetype,
     struct FsFile *created
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

    struct Inode inode = {
        .file_size   = 0,
        .links_count = 1,
        .blocks      = {0}
    };

    if (write_inode(file, superblock, &inode, inode_id)) {
        fprintf(stderr, "Failed to write the new file inode");
        return RETURN_ERROR;
    }

    if (created) {
        *created = (struct FsFile){
            .inode_id = inode_id,
            .inode    = inode,
            .filetype = filetype
        };
        if (path_join(fsfile->fullname, args, &created->fullname))
            return RETURN_ERROR;
    }

    if (write_inode(file, superblock, &fsfile->inode, fsfile->inode_id)) {
        fprintf(stderr, "Failed to write the current directory inode");
        if (created)
            free(created->fullname);
        return RETURN_ERROR;
    }

    if (write_superblock(superblock, file)) {
        fprintf(stderr, "Failed to write the superblock");
        if (created)
            free(created->fullname);
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
    return create_file(superblock, fsfile, file, args, FILETYPE_FILE, NULL);
}

int mkdir(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    struct FsFile dir_fsfile;
    if (create_file(superblock, fsfile, file, args, FILETYPE_DIRECTORY, &dir_fsfile))
        return RETURN_ERROR;

    struct DirectoryEntry entries[] = {
        {
            .inode_id = dir_fsfile.inode_id,
            .filetype = FILETYPE_DIRECTORY,
            .name_len = 1,
            .name     = "."
        },
        {
            .inode_id = fsfile->inode_id,
            .filetype = FILETYPE_DIRECTORY,
            .name_len = 2,
            .name     = ".."
        }
    };

    if (write_contents(file, superblock, &dir_fsfile, (const uint8_t*)entries, DIRECTORY_ENTRY_SIZE * 2)) {
        fprintf(stderr, "Failed to create . and .. for the created directory\n");
        free(dir_fsfile.fullname);
        return RETURN_ERROR;
    }

    ++dir_fsfile.inode.links_count;
    if (write_inode(file, superblock, &dir_fsfile.inode, dir_fsfile.inode_id)) {
        fprintf(stderr, "Failed to rewrite the old inode for the created directory\n");
        free(dir_fsfile.fullname);
        return RETURN_ERROR;
    }

    ++fsfile->inode.links_count;
    if (write_inode(file, superblock, &fsfile->inode, fsfile->inode_id)) {
        fprintf(stderr, "Failed to rewrite the old inode for the current directory\n");
        free(dir_fsfile.fullname);
        return RETURN_ERROR;
    }

    free(dir_fsfile.fullname);
    return RETURN_SUCCESS;
}

int cd(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    if (args == NULL) {
        fprintf(stderr, "[openfs] Args cannot be NULL for this command\n");
        return RETURN_ERROR;
    }

    struct FsFile found_file;
    if (find_file(file, superblock, fsfile, args, &found_file))
        return RETURN_ERROR;

    if (found_file.filetype != FILETYPE_DIRECTORY) {
        fprintf(stderr, "Is not a directory\n");
        return RETURN_ERROR;
    }

    *fsfile = found_file;
    return RETURN_SUCCESS;
}

int stat(
     struct Superblock *superblock,
     struct FsFile *fsfile,
     FILE *file,
     char* args
) {
    if (args == NULL) {
        fprintf(stderr, "[openfs] Args cannot be NULL for this command\n");
        return RETURN_ERROR;
    }

    struct FsFile found_file;
    if (find_file(file, superblock, fsfile, args, &found_file))
        return RETURN_ERROR;

    printf(
        "Full filename: %s\n"
        "Inode #%d\n"
        "Filetype: %s\n"
        "Filesize: %d\n"
        "Links count: %d\n",
        found_file.fullname,
        found_file.inode_id,
        filetype_str(found_file.filetype),
        found_file.inode.file_size,
        found_file.inode.links_count
    );

    return RETURN_SUCCESS;
}

const command_func_ptr commands[] = {
    &help,
    &echo,
    &ls,
    &touch,
    &mkdir,
    &cd,
    &stat
};
const char* command_names[] = {
    "help",
    "echo",
    "ls",
    "touch",
    "mkdir",
    "cd",
    "stat"
};
const size_t n_commands = sizeof(command_names) / sizeof(const char*);
