#include "misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "directory_entry.h"

int path_join(const char *p1, const char *p2, char **result) {
    *result = malloc(strlen(p1) + 1 + strlen(p2));
    if (!result) {
        fprintf(stderr, "Failed to allocate memory for path joining\n");
        return 1;
    }

    strcpy(*result, p1);
    if (strcmp(p2, ".") == 0) {
        return 0;
    } else if (strcmp(p2, "..") == 0) {
        if (strcmp(p1, "/") == 0)
            return 0;

        for (int i = strlen(p1) - 1; i > 0; --i) {
            if ((*result)[i - 1] == '/') {
                (*result)[i] = '\0';
                return 0;
            }
        }
    } else if (strcmp(p1, "/") != 0) {
        (*result)[strlen(p1)] = '/';
        strcpy(*result + strlen(p1) + 1, p2);
    } else {
        strcpy(*result + strlen(p1), p2);
    }

    return 0;
}

const char* filetype_str(const uint8_t filetype) {
    char *filetype_cstr;
    switch (filetype) {
    case FILETYPE_DIRECTORY:
        filetype_cstr = "DIR";
        break;
    case FILETYPE_FILE:
        filetype_cstr = "FILE";
        break;
    default:
        filetype_cstr = "???";
        break;
    }

    return filetype_cstr;
}
