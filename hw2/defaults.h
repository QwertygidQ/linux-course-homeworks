#ifndef DEFAULTS_H
#define DEFAULTS_H

#include <stdlib.h>
#include <stdint.h>

const size_t   DEFAULT_BOOT_OFFSET  = 1024; // bytes
const uint16_t DEFAULT_MAGIC        = 0xEF53;
const uint32_t DEFAULT_BLOCK_SIZE   = 8; // bytes
const uint32_t DEFAULT_TOTAL_BLOCKS = 1024;
const uint32_t DEFAULT_INODE_SIZE   = 0; // bytes
const uint32_t DEFAULT_TOTAL_INODES = 0;

#endif
