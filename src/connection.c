#include "buffer.h"
#include "connection.h"
#include "route.h"
#include "worker.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// NOTE: Milliseconds
#define TIMEOUT 5000
#define READ_CHUNK 4096
#define WRITE_CHUNK 4096

static void timeout_callback(void *data)
{
    struct connection *c = data;
    close_connection(c);
}

static inline void read_data(struct connection *c)
{
    buffer_t *buf = c->buffer;
    while (1) {
        size_t size = buf->size + READ_CHUNK;
        if (size > buf->cap) {
            if (resize_buffer(buf, size) != OK) {
                fprintf(stderr, "Connection: read_data resize_buffer error\n");
                c->state = CONNECTION_STATE_ERROR;
                close_connection(c);
                return;
            }
        }

        ssize_t num_read = recv(c->fd, buf->data + buf->size, READ_CHUNK, 0);
        if (num_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct request *req = &c->req;

                int ret = parse_request(req);
                if (ret == REQ_OK) {
                    // Request receive complete. Parse
                    const char *uri = buf->data + req->uri.off;
                    const struct route *route = match_route(req->method, uri, (size_t)req->uri.len);
                    if (route) {
                        // Route found, call the callback
                        route->cb(req, route->data);
                    } else {
                        // Route not found
                        // TODO: a better 404 callback
                        route_404(req);
                    }
                } else if (ret == REQ_ERR) {
                    // Bad request.
                    // TODO: Send HTTP bad request?
                    close_connection(c);
                } else {
                    // Request underflow, need more data. Keep reading.
                    continue;
                }

                break;
            } else {
                perror("Connection: read_data recv error\n");
                c->state = CONNECTION_STATE_ERROR;
                close_connection(c);
                break;
            }
        } else if (num_read == 0) {
            c->state = CONNECTION_STATE_CLOSED;
            close_connection(c);
            break;
        } else {
            buf->size += (size_t)num_read;
        }
    }
}

// NOTE: This function might close the connection if it encounters an error.
// Wherever this gets called from, should not touch the connection afterwards
// TODO: This could be slightly more elegant.
static inline void send_data(struct connection *c)
{
    buffer_t *buf = c->buffer;
    while (1) {
        size_t rem = buf->size - c->write_bytes;
        size_t len = (rem < WRITE_CHUNK) ? rem : WRITE_CHUNK;
        ssize_t num_sent = send(c->fd, buf->data + c->write_bytes, len, 0);
        if (num_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                perror("Connection: send_data send error");
                close_connection(c);
                break;
            }
        } else {
            c->write_bytes += (size_t)num_sent;
            if (c->write_bytes == buf->size) {
                if (c->flags & CONNECTION_FLAG_KEEPALIVE) {
                    // Reset the connection back to IN state
                    begin_read(c);
                } else {
                    // No keep-alive and the request has been served. Close connection
                    close_connection(c);
                }
                break;
            }
        }
    }
}

static void io_callback(int fd, uint32_t events, void *data)
{
    UNUSED(fd);
    struct connection *c = data;
    switch (c->state) {
        case CONNECTION_STATE_IN: {
            if (events & EPOLLIN) {
                read_data(c);
            } else {
                fprintf(stderr, "Connection: io_callback state mismatch\n");
                close_connection(c);
            }
        } break;

        case CONNECTION_STATE_OUT: {
            if (events & EPOLLOUT) {
                send_data(c);
            } else {
                fprintf(stderr, "Connection: io_callback state mismatch\n");
                close_connection(c);
            }
        } break;

        case CONNECTION_STATE_CLOSED:
        case CONNECTION_STATE_ERROR:
        default: { // NOLINT
            close_connection(c);
        } break;
    }
}

int accept_connection(struct worker *worker)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    int fd = accept(worker->sock, (struct sockaddr *)&addr, &len);
    if (fd < 0) {
        perror("Connection: accept");
        return ERR;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("Connection: fcntl");
        close(fd);
        return ERR;
    }

    struct connection *c = calloc(1, sizeof *c);
    if (c) {
        buffer_t *buffer = create_buffer();
        if (!buffer) {
            fprintf(stderr, "Connection: add_event error\n");
            close(fd);
            free(c);
            return ERR;
        }

        c->fd = fd;
        c->worker = worker;
        c->buffer = buffer;
        list_add_entry_tail(&worker->conns, c, node);
        begin_read(c);
    } else {
        perror("Connection: calloc");
        close(fd);
        return ERR;
    }
    return OK;
}

void close_connection(struct connection *c)
{
    struct worker *worker = c->worker;
    worker->num_conns--;
    del_event(worker->base, &c->event);
    del_timer(worker->base, &c->timer);
    close(c->fd);
    list_del_entry(c, node);
    free_buffer(c->buffer);
    free(c);
}

// NOTE: This function does not clear the buffer, because it can be called from begin_send
// at which point the buffer is filled with the data to be sent.
int reset_connection(struct connection *c)
{
    struct worker *worker = c->worker;

    zero_struct(c->req);
    c->write_bytes = 0;

    // Reset the timer
    if (c->timer.flags & TIMER_FLAG_ACTIVE)
        del_timer(worker->base, &c->timer);

    c->timer = make_timer(TIMEOUT, TIMER_FLAG_ONESHOT, timeout_callback, c);
    if (add_timer(worker->base, &c->timer) != OK) {
        fprintf(stderr, "Connection: reset_connection add_timer error\n");
        return ERR;
    }

    if (c->event.flags & EVENT_FLAG_ACTIVE)
        del_event(worker->base, &c->event);

    return OK;
}

void begin_read(struct connection *c)
{
    struct worker *worker = c->worker;

    if (reset_connection(c) != OK) {
        close_connection(c);
        return;
    }

    clear_buffer(c->buffer);
    c->state = CONNECTION_STATE_IN;
    c->event = make_event(c->fd, EPOLLIN|EPOLLET, io_callback, c);
    if (add_event(worker->base, &c->event) != OK) {
        fprintf(stderr, "Connection: begin_read add_event error\n");
        close_connection(c);
        return;
    }
}

void begin_send(struct connection *c)
{
    struct worker *worker = c->worker;

    if (reset_connection(c) != OK) {
        close_connection(c);
        return;
    }

    c->state = CONNECTION_STATE_OUT;
    c->event = make_event(c->fd, EPOLLOUT|EPOLLET, io_callback, c);
    if (add_event(worker->base, &c->event) != OK) {
        fprintf(stderr, "Connection: connection_begin_send add_event error\n");
        close_connection(c);
        return;
    }

    send_data(c);
}
