#ifndef DIRECTORY_ENTRY_H
#define DIRECTORY_ENTRY_H

#include <stdint.h>

#include "constants.h"

struct DirectoryEntry {
    uint32_t inode_id;
    uint8_t  filetype;
    uint8_t  name_len;
    char     name[MAX_FILENAME_LEN];
};

#define DIRECTORY_ENTRY_SIZE sizeof(struct DirectoryEntry)

#endif
