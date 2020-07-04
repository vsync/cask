#ifndef FILE_H
#define FILE_H

#include "common.h"

#define FILE_ERROR_NOT_FOUND (1 << 0)
#define FILE_ERROR_NO_PERM (1 << 1)
#define FILE_ERROR_UNKNOWN (1 << 2)

struct file
{
    size_t size;
    void *data;
    int error;
};

struct file map_file(const char *path);
void unmap_file(struct file *file);

#endif
