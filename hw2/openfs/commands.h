#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdlib.h>
#include <stdio.h>

#include "../common/superblock.h"
#include "../common/inode.h"

typedef int (*command_func_ptr)(struct Superblock*, struct Inode*, FILE*, char*);

extern const command_func_ptr commands[];
extern const char*            command_names[];
extern const size_t           n_commands;

#define MAX_COMMAND_LEN 255

#define RETURN_SUCCESS  0
#define RETURN_ERROR    1
#define RETURN_CRITICAL 2

#endif
