#include "inode_ops.h"

#include "constants.h"

static int seek_to_inode(
     FILE *file,
     const struct Superblock *superblock,
     const size_t inode_id
) {
    if (inode_id == 0 || inode_id > superblock->total_inodes) {
        fprintf(stderr, "Invalid inode id\n");
        return 1;
    }

    if (fseek(file, BOOT_OFFSET + superblock->size, SEEK_SET)){
        fprintf(stderr, "Failed to seek to the beginning of the inode table\n");
        return 1;
    }
    if (fseek(file, (inode_id - 1) * INODE_SIZE, SEEK_CUR)) {
        fprintf(stderr, "Failed to seek to the inode\n");
        return 1;
    }

    return 0;
}

int write_inode(
     FILE *file,
     const struct Superblock *superblock,
     const struct Inode *inode,
     const size_t inode_id
) {
    if (seek_to_inode(file, superblock, inode_id))
        return 1;

    if (fwrite(inode, sizeof(*inode), 1, file) != 1) {
        fprintf(stderr, "Failed to write the inode\n");
        return 1;
    }

    return 0;
}

int read_inode (
     FILE *file,
     const struct Superblock *superblock,
     struct Inode *inode,
     const size_t inode_id
) {
    if (seek_to_inode(file, superblock, inode_id))
        return 1;

    if (fread(inode, sizeof(*inode), 1, file) != 1) {
        fprintf(stderr, "Failed to read the inode\n");
        return 1;
    }

    return 0;
}
