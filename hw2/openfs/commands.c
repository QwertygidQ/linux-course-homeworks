#include "commands.h"

int help(struct Superblock *superblock, struct Inode* current_dir, FILE *file, char* args) {
    printf(
        "help -- shows this message\n"
        "ls -- lists all files in the current directory\n"
        "echo MESSAGE -- prints out MESSAGE\n"
    );

    return RETURN_SUCCESS;
}

int echo(struct Superblock *superblock, struct Inode* current_dir, FILE *file, char* args) {
    if (args)
        printf("%s\n", args);
    else
        printf("\n");

    return RETURN_SUCCESS;
}

const command_func_ptr commands[] = {
    &help,
    &echo
};
const char* command_names[] = {
    "help",
    "echo"
};
const size_t n_commands = sizeof(command_names) / sizeof(const char*);
