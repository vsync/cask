#include "cask.h"
#include "worker.h"
#include "event.h"
#include "connection.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

static void destroy_worker(struct worker *worker)
{
    fprintf(stderr, "Worker #%ld preparing shutdown...\n", worker->id);
    struct list *iter, *next;
    list_for_each_safe(iter, next, &worker->conns) {
        struct connection *c = list_entry(iter, struct connection, node);
        close_connection(c);
    }
    fprintf(stderr, "Worker #%ld connections closed, shutting down...\n", worker->id);

    close(worker->sock);
    destroy_event_base(worker->base);

    pthread_mutex_lock(&g_cask->worker_lock);
    g_cask->num_workers--;
    list_del_entry(worker, node);
    pthread_mutex_unlock(&g_cask->worker_lock);
    free(worker);
}

static void *worker_proc(void *arg)
{
    void *ret = 0;
    struct worker *worker = arg;
    struct event_base *base = worker->base;
    worker->running = true;
    fprintf(stderr, "Worker #%ld started\n", worker->id);
    while(worker->running) {
        if (event_base_iter(base) != OK) {
            fprintf(stderr, "Worker: event_base_iter error\n");
            worker->running = false;
            ret = (void *)1;
            break;
        }
    }
    destroy_worker(worker);
    pthread_exit(ret);
}

static void on_accept(int fd, uint32_t events, void *data)
{
    UNUSED(fd);
    struct worker *worker = data;
    if (events & EPOLLIN) {
        if (accept_connection(worker) != OK) {
            fprintf(stderr, "Worker: accept_connection error\n");
        } else {
            // NOTE: The decrement on close is done in connection.c
            worker->num_conns++;
        }
    } else {
        fprintf(stderr, "Worker: on_accept unknown event\n");
    }
}

int start_worker(void)
{
    struct addrinfo *ai = g_cask->ai;
    int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock < 0) {
        perror("Worker: socket");
        return ERR;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("Worker: setsockopt");
        close(sock);
        return ERR;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        perror("Worker: setsockopt");
        close(sock);
        return ERR;
    }
    if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
        perror("Worker: bind");
        close(sock);
        return ERR;
    }
    if (listen(sock, SOMAXCONN) < 0) {
        perror("Worker: listen");
        close(sock);
        return ERR;
    }

    struct worker *worker = calloc(1, sizeof *worker);
    if (!worker) {
        perror("Worker: calloc");
        close(sock);
        return ERR;
    }

    struct event_base *base = create_event_base();
    if (!base) {
        fprintf(stderr, "Worker: create_event_base error\n");
        close(sock);
        free(worker);
        return ERR;
    }

    worker->event = make_event(sock, EPOLLIN|EPOLLET, on_accept, worker);
    if (add_event(base, &worker->event) != OK) {
        fprintf(stderr, "Worker: add_event error\n");
        close(sock);
        destroy_event_base(base);
        free(worker);
        return ERR;
    }

    worker->sock = sock;
    worker->base = base;
    LIST_INIT_HEAD(worker->conns);

    pthread_mutex_lock(&g_cask->worker_lock);
    worker->id = g_cask->current_id++;
    list_add_entry_tail(&g_cask->workers, worker, node);

    int ret = OK;
    if (pthread_create(&worker->thread, NULL, worker_proc, worker)) {
        perror("Worker: pthread_create");
        list_del_entry(worker, node);
        close(sock);
        destroy_event_base(base);
        free(worker);
        ret = ERR;
    }
    g_cask->num_workers++;
    pthread_mutex_unlock(&g_cask->worker_lock);
    return ret;
}

void shutdown_worker(struct worker *worker)
{
    if (worker->running) {
        worker->running = false;
        pthread_join(worker->thread, NULL);
    }
}
