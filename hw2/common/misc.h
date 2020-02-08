#ifndef MISC_H
#define MISC_h

#include <stdint.h>

int         path_join(const char *p1, const char *p2, char **result);
const char* filetype_str(const uint8_t filetype);

#endif
