#ifndef CONNECTION_H
#define CONNECTION_H

#include "list.h"
#include "event.h"
#include "request.h"
#include <arpa/inet.h>

typedef struct buffer_t buffer_t;

struct worker;

enum connection_state
{
    CONNECTION_STATE_IN,
    CONNECTION_STATE_OUT,
    CONNECTION_STATE_CLOSED,
    CONNECTION_STATE_ERROR
};

#define CONNECTION_FLAG_KEEPALIVE (1 << 0)

struct connection
{
    struct list node;
    struct event event;
    struct timer timer;
    
    struct worker *worker;

    int fd;
    struct sockaddr_storage addr;
    enum connection_state state;

    int flags;

    buffer_t *buffer;
    size_t write_bytes;

    struct request req;
};

int accept_connection(struct worker *worker);
void close_connection(struct connection *c);
int reset_connection(struct connection *c);
void begin_read(struct connection *c);
void begin_send(struct connection *c);

#endif
