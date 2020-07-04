#include "buffer.h"
#include <stdlib.h>

buffer_t *create_buffer(void)
{
    return calloc(1, sizeof(buffer_t));
}

void free_buffer(buffer_t *buf)
{
    free(buf->data);
    free(buf);
}

int resize_buffer(buffer_t *buf, size_t size)
{
    assert(buf);
    if (size == 0) {
        buf->size = 0;
        free(buf->data);
        buf->data = NULL;
    } else {
        void *tmp = realloc(buf->data, size);
        if (!tmp)
            return ERR;

        if (buf->size > size)
            buf->size = size;
        buf->cap = size;
        buf->data = tmp;
    }
    return OK;
}

int push_buffer(buffer_t *buf, const char *data, size_t size)
{
    if (buf->size + size > buf->cap) {
        if (resize_buffer(buf, buf->size + size) != OK)
            return ERR;
    }
    memcpy(buf->data + buf->size, data, size); // NOLINT [C11 Annex K]
    buf->size += size;
    return OK;
}
