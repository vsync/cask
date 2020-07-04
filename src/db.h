#ifndef DB_H
#define DB_H

#include "common.h"

struct db;

typedef uint64_t dbid_t;

struct db_params
{
    size_t num_buckets;
};

struct db *open_db(const char *path, const struct db_params *params);
void close_db(struct db *db);
int db_insert(struct db *db, const void *val, uint32_t vlen, dbid_t *result);
void *db_get(struct db *db, dbid_t id, uint32_t *vlen);

#endif
