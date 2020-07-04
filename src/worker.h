#ifndef WORKER_H
#define WORKER_H

#include "common.h"
#include "event.h"
#include "list.h"
#include <pthread.h>

struct event_base;

// NOTE: There should probably be proper ipc with the main thread, for worker termination,
// and data that can be accessed from the main thread, such as the connection count.

struct worker
{
    struct list node;
    pthread_t thread;
    thread_id id;

    int sock;
    volatile bool running;

    struct event event;

    struct list conns;

    // See note above
    // NOTE: This doesn't prevent the data race between the main thread and the worker, when
    // reading this variable. However, it's not *really* vital to prevent said data race.
    volatile size_t num_conns;

    struct event_base *base;
};

int start_worker(void);
void shutdown_worker(struct worker *worker);

#endif
