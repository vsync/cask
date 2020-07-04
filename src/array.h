#ifndef ARRAY_H
#define ARRAY_H

#include "common.h"
#include <stdlib.h>

typedef struct array_header_t
{
    size_t sz;
    size_t cap;
} array_header_t;

#define array_header(a) ((array_header_t *)(void *)((uint8_t *)(a) - sizeof(array_header_t)))
#define array_size(a) ((a) ? array_header((a))->sz : 0)
#define array_cap(a) ((a) ? array_header((a))->cap : 0)
#define array_full(a) (array_size((a)) == array_cap((a)))
#define array_push_back(a, item) \
    array_full((a)) ? (a) = array_resize((a), array_size((a))*2u, sizeof(*(a))) : 0, \
    (a)[array_header((a))->sz++] = item

#define array_push_back_n(a, items, n) \
    do { \
        if (array_size((a))+(n) > array_cap((a))) \
            (a) = array_resize((a), next_pot64(array_size((a))+(n)), sizeof(*(a))); \
        memcpy(&a[array_size((a))], (items), sizeof(*(a))*(n)); \
        array_header((a))->sz += (n); \
    } while(0)

#define array_insert(a, item, i) \
    do { \
        if (array_full((a))) (a) = array_resize((a), array_size((a))*2u, sizeof(*(a))); \
        if (array_size((a))) \
            memmove(&a[(i)+1], &a[(i)], (array_size((a))-i)*sizeof(*(a))); \
        (a)[(i)] = item; \
        array_header((a))->sz++; \
    } while(0)

#define array_pop_back(a) \
    if (array_size((a))) do { \
        array_header((a))->sz--; \
    } while(0)

#define array_remove(a, i) \
    do { \
        assert((a) && (array_size((a)) > (i))); \
        if ((i) < array_size((a))-1) \
            memmove(&a[(i)], &a[(i)+1], (array_size((a))-(i)-1)*sizeof(*(a))); \
        array_header((a))->sz--; \
    } while(0)

#define array_free(a) \
    if ((a)) do { \
        free(array_header((a))); \
    } while(0)

#define array_clear(a) \
    ((a)) ? array_header((a))->sz = 0 : 0

#define array_shrink(a) \
    (array_cap((a)) > array_size((a))) ? (a) = array_resize((a), array_size((a)), sizeof(*(a))) : 0

#define array_ensure(a, sz) \
    ((array_size((a)) < (sz)) ? (a) = array_resize((a), (sz), sizeof(*(a))) : 0)

static inline void *array_resize(void *arr, size_t sz, size_t elmsz)
{
    if (sz == 0)
        sz = 1;
    array_header_t *header = NULL;
    if (arr) {
        header = realloc(array_header(arr), sizeof *header + sz*elmsz);
    } else {
        header = malloc(sizeof *header + sz*elmsz);
        header->sz = 0;
    }
    header->cap = sz;
    return (void *)((uint8_t *)header + sizeof *header);
}

#endif
