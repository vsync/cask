#ifndef CASK_H
#define CASK_H

#include "common.h"
#include "list.h"
#include <netdb.h>
#include <pthread.h>

struct cask
{
    volatile bool running;
    uint64_t started_at;

    struct addrinfo *ai;

    pthread_mutex_t worker_lock;
    thread_id current_id;
    uint32_t num_workers;
    struct list workers;

    int ipcfd;
};

extern struct cask *g_cask;

#endif
