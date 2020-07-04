#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint64_t thread_id;

#define ERR (-1)
#define OK (0)

#define UNUSED(x) (void)(x)
#define FALLTHROUGH __attribute__((fallthrough))

#define assert(x)

#define container_of(ptr, type, member) \
    ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

#define zero_struct(x) memset(&(x), 0x00, sizeof((x))) // NOLINT [C11 Annex K]
#define zero_structp(x) if ((x)) do { memset((x), 0x00, sizeof(*(x))); } while(0) // NOLINT [C11 Annex K]

#endif
