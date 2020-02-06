#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MAGIC                 0xEF53
#define BOOT_OFFSET           1024   // bytes

#define INODE_BLOCK_COUNT     14
#define INDIRECT_BLOCK        12
#define DOUBLE_INDIRECT_BLOCK 13

#define MAX_FILENAME_LEN      255

#define FILETYPE_FILE         0
#define FILETYPE_DIRECTORY    1

#endif
