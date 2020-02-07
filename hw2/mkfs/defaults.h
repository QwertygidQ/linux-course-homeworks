#ifndef DEFAULTS_H
#define DEFAULTS_H

// must be divisible by 8 (32 bits) for indirect addressing, must be greater than DIRECTORY_ENTRY_SIZE
#define DEFAULT_BLOCK_SIZE   128   // bytes

#define DEFAULT_TOTAL_BLOCKS 8192  // excluding the superblock
#define DEFAULT_TOTAL_INODES 1024

#endif
