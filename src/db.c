#include "db.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define DBID_FREE 0xFFFFFFFFFFFFFFFF

#define REC_HDR_SIZE offsetof(struct record, offset)

#pragma pack(push, 1)

struct header
{
    dbid_t id;
    size_t num_buckets;
};

struct db
{
    int fd;

    pthread_rwlock_t lock;

    struct header *meta;
    dbid_t *buckets;
    void *map;
};

struct record
{
    uint32_t vlen;
    dbid_t id;
    dbid_t next;

    dbid_t offset;

    const char *val;
};
#pragma pack(pop)

static inline uint64_t get_file_size(int fd)
{
    struct stat fs;
    if (fstat(fd, &fs) < 0) return 0;
    return (uint64_t)fs.st_size;
}

static inline int write_record(int fd, const struct record *rec)
{
    size_t size = REC_HDR_SIZE + rec->vlen;
    uint8_t *buf = malloc(size);
    if (!buf) return ERR;

    uint8_t *dst = buf;
    memcpy(dst, rec, REC_HDR_SIZE); // NOLINT [C11 Annex K]
    dst += REC_HDR_SIZE;
    memcpy(dst, rec->val, rec->vlen); // NOLINT [C11 Annex K]

    int ret = OK;
    if (pwrite(fd, buf, size, (off_t)rec->offset) < 0)
        ret = ERR;
    free(buf);
    return ret;
}

struct db *open_db(const char *path, const struct db_params *params)
{
    struct db *db = calloc(1, sizeof *db);
    if (!db) return NULL;
    if (pthread_rwlock_init(&db->lock, NULL)) {
        free(db);
        return NULL;
    }

    if (access(path, F_OK) < 0) {
        int fd = open(path, O_RDWR|O_CREAT, S_IRWXU);
        if (fd < 0) {
            // TODO: Logging
            goto error;
        }

        size_t size = sizeof(struct header) + sizeof(dbid_t) * params->num_buckets;
        if (ftruncate(fd, (off_t)size) < 0) {
            // TODO: Logging
            close(fd);
            goto error;
        }

        void *map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            // TODO: Logging
            close(fd);
            goto error;
        }

        struct header *meta = map;
        meta->id = 0;
        meta->num_buckets = params->num_buckets;

        dbid_t *buckets = (dbid_t *)(void *)((uint8_t *)map + sizeof(struct header));
        memset(buckets, 0xff, sizeof(dbid_t) * params->num_buckets); // NOLINT [C11 Annex K]

        db->fd = fd;
        db->meta = meta;
        db->buckets = buckets;
        db->map = map;
    } else {
        int fd = open(path, O_RDWR);
        if (fd < 0) {
            // TODO: Logging
            goto error;
        }

        struct header header = {0};
        if (read(fd, &header, sizeof header) != sizeof header) {
            // TODO: Logging
            close(fd);
            goto error;
        }

        size_t size = sizeof(struct header) + sizeof(dbid_t) * header.num_buckets;
        void *map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            // TODO: Logging
            close(fd);
            goto error;
        }

        db->fd = fd;
        db->meta = map;
        db->buckets = (dbid_t *)(void *)((uint8_t *)map + sizeof(struct header));
        db->map = map;
    }

    return db;

error:
    pthread_rwlock_destroy(&db->lock);
    free(db);
    return NULL;
}

void close_db(struct db *db)
{
    if (db->map != MAP_FAILED) {
        size_t size = sizeof(struct header) + sizeof(dbid_t) * db->meta->num_buckets;
        munmap(db->map, size);
    }
    if (db->fd >= 0)
        close(db->fd);

    free(db);
}

int db_insert(struct db *db, const void *val, uint32_t vlen, dbid_t *result)
{
    int ret = OK;
    if (pthread_rwlock_wrlock(&db->lock))
        return ERR;

    dbid_t id = db->meta->id++;
    dbid_t index = id % db->meta->num_buckets;
    dbid_t pos = get_file_size(db->fd);

    dbid_t offset = db->buckets[index];
    if (offset == DBID_FREE) {
        // Empty bucket, update it to point to the new entry,
        // which will act as the head of the list
        db->buckets[index] = pos;
    } else {
        // Bucket is not free. Get the last link in the list.
        while (1) {
            struct record rec;
            if (pread(db->fd, &rec, REC_HDR_SIZE, (off_t)offset) != REC_HDR_SIZE) {
                // TODO: Logging / Error handling
                ret = ERR;
                goto out;
            }

            if (rec.next != DBID_FREE) {
                offset = rec.next;
            } else {
                break;
            }
        }

        // Update the last link to point to the new entry (that is to be added)
        if (pwrite(db->fd, &pos, sizeof pos, (off_t)(offset + offsetof(struct record, offset))) != sizeof offset) {
            ret = ERR;
            goto out;
        }
    }

    *result = id;

    struct record rec = {0};
    rec.vlen = vlen;
    rec.id = id;
    rec.next = DBID_FREE;
    rec.offset = pos;
    rec.val = val;
    ret = write_record(db->fd, &rec);
out:
    pthread_rwlock_unlock(&db->lock);
    return ret;
}

void *db_get(struct db *db, dbid_t id, uint32_t *vlen)
{
    void *ret = NULL;
    if (pthread_rwlock_rdlock(&db->lock))
        return NULL;

    *vlen = 0;
    dbid_t index = id % db->meta->num_buckets;
    dbid_t offset = db->buckets[index];
    if (offset == DBID_FREE)
        goto out;

    while (1) {
        struct record rec;
        if (pread(db->fd, &rec, REC_HDR_SIZE, (off_t)offset) != REC_HDR_SIZE) {
            // TODO: Logging / Error handling
            goto out;
        }

        if (rec.id == id) {
            void *buf = malloc(rec.vlen);
            if (!buf) {
                // TODO: Logging / Error handling
                goto out;
            }

            if (pread(db->fd, buf, rec.vlen, (off_t)(offset + REC_HDR_SIZE)) != rec.vlen) {
                // TODO: Logging / Error handling
                free(buf);
                goto out;
            }

            *vlen = rec.vlen;
            ret = buf;
            goto out;
        }

        if (rec.next != DBID_FREE) {
            offset = rec.next;
        } else {
            break;
        }
    }
out:
    pthread_rwlock_unlock(&db->lock);
    return ret;
}
