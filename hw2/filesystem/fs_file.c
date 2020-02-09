#include "fs_file.h"

#include <stdlib.h>
#include <string.h>

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

int remove_file(
     FILE *file,
     struct Superblock *superblock,
     struct FsFile *directory,
     const char *filename
) {
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        fprintf(stderr, "Cannot remove . or ..\n");
        return 1;
    }

    struct DirectoryEntry *entries, entry;
    if (load_contents(file, superblock, directory, (uint8_t**)&entries)) {
        fprintf(stderr, "Failed to load directory contents\n");
        return 1;
    }

    const size_t n_entries = directory->inode.file_size / DIRECTORY_ENTRY_SIZE;
    int index = -1;
    for (size_t i = 0; i < n_entries; ++i) {
        if (strcmp(entries[i].name, filename) == 0) {
            entry = entries[i];
            index = i;
            break;
        }
    }

    if (index == -1) {
        fprintf(stderr, "File not found in current directory\n");
        free(entries);
        return 1;
    }

    entries[index] = entries[n_entries - 1];

    if (
         write_contents(
             file,
             superblock,
             directory,
             (const uint8_t*)entries,
             (n_entries - 1) * DIRECTORY_ENTRY_SIZE
         )
    ) {
        fprintf(stderr, "Failed to remove directory entry\n");
        free(entries);
        return 1;
    }

    free(entries);

    if (entry.filetype == FILETYPE_DIRECTORY)
        --directory->inode.links_count; // ..

    if (write_inode(file, superblock, &directory->inode, directory->inode_id)) {
        fprintf(stderr, "Failed to write directory inode\n");
        return 1;
    }

    struct FsFile found_file = {
        .inode_id = entry.inode_id,
        .filetype = entry.filetype
    };
    if (read_inode(file, superblock, &found_file.inode, found_file.inode_id)) {
        fprintf(stderr, "Failed to read inode\n");
        return 1;
    }
    --found_file.inode.links_count; // removed from parent directory

    if (found_file.filetype == FILETYPE_DIRECTORY) {
        if (load_contents(file, superblock, &found_file, (uint8_t**)&entries)) {
            fprintf(stderr, "Failed to load directory contents\n");
            return 1;
        }

        const size_t dir_len = found_file.inode.file_size / DIRECTORY_ENTRY_SIZE;
        for (size_t i = 0; i < dir_len; ++i) {
            if (strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0) {
                printf("Removing nested file %s\n", entries[i].name);
                if (remove_file(file, superblock, &found_file, entries[i].name)) {
                    fprintf(stderr, "Failed to remove nested directory\n");
                    free(entries);
                    return 1;
                }
            }
        }

        --found_file.inode.links_count; // .
        free(entries);
    }

    if (found_file.inode.links_count == 0) {
        printf("No more links to the file, clearing\n");
        if (clear_file(file, superblock, &found_file)) {
            fprintf(stderr, "Failed to clear file\n");
            return 1;
        }
    } else {
        printf("%d links left\n", found_file.inode.links_count);
        if (write_inode(file, superblock, &found_file.inode, found_file.inode_id)) {
            fprintf(stderr, "Failed to write inode\n");
            return 1;
        }
    }

    return 0;
}
