#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"

typedef struct buffer_t
{
    size_t size;
    size_t cap;
    char *data;
} buffer_t;

buffer_t *create_buffer(void);
void free_buffer(buffer_t *buf);
int resize_buffer(buffer_t *buf, size_t size);
int push_buffer(buffer_t *buf, const char *data, size_t size);

static inline void clear_buffer(buffer_t *buf)
{
    buf->size = 0;
}

#endif
