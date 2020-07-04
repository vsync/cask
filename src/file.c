#include "file.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

struct file map_file(const char *path)
{
    struct file res = {0};
    struct stat fs;
    if (stat(path, &fs)) {
        switch (errno) {
            case ENOENT: {
                res.error = FILE_ERROR_NOT_FOUND;
            } break;

            case EACCES: {
                res.error = FILE_ERROR_NO_PERM;
            } break;

            default: {
                res.error = FILE_ERROR_UNKNOWN;
            } break;
        }
        return res;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        switch (errno) {
            case ENOENT: {
                res.error = FILE_ERROR_NOT_FOUND;
            } break;

            case EACCES: {
                res.error = FILE_ERROR_NO_PERM;
            } break;

            default: {
                res.error = FILE_ERROR_UNKNOWN;
            } break;
        }
        return res;
    }

    void *data = mmap(NULL, (size_t)fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        res.error = FILE_ERROR_UNKNOWN;
        return res;
    }
    close(fd);
    res.size = (size_t)fs.st_size;
    res.data = data;
    return res;
}

void unmap_file(struct file *file)
{
    if (file->data)
        munmap(file->data, file->size);
}
