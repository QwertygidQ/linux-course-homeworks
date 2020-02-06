#ifndef WRITE_OPS_H
#define WRITE_OPS_H

#include <stdlib.h>
#include <stdio.h>

#include "superblock.h"

int read_blocks(
    FILE *file,
    const struct Superblock *superblock,
    const uint32_t *block_ids,
    const size_t n_block_ids,
    void *ptr,
    const size_t ptr_size
);

int write_blocks(
    FILE *file,
    const struct Superblock *superblock,
    const uint32_t *block_ids,
    const size_t n_block_ids,
    const void *ptr,
    const size_t ptr_size
);

#endif
