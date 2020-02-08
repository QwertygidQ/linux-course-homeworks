#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdlib.h>
#include <stdio.h>

#include "../filesystem/fs_file.h"
#include "../filesystem/inode.h"
#include "../filesystem/superblock.h"

typedef int (*command_func_ptr)(struct Superblock*, struct FsFile *fsfile, FILE*, char*);

extern const command_func_ptr commands[];
extern const char*            command_names[];
extern const size_t           n_commands;

#define MAX_COMMAND_LEN 255
#define MAX_EDIT_LEN    10000

#define RETURN_SUCCESS  0
#define RETURN_ERROR    1
#define RETURN_CRITICAL 2

#endif
