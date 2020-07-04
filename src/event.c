#include "event.h"
#include "rbtree.h"
#include "util.h"
#include <stdlib.h>
#include <unistd.h>

#define MAX_IO_EVENTS 64
#define IO_TIMEOUT 10

struct event_base
{
    int epollfd;
    struct rbtree timers;
};

static inline void update_trigger(struct timer *t)
{
    uint64_t now = get_time();
    t->trigger = now + (uint64_t)t->interval * 1000000ULL;
}

static inline int timer_cmp(const struct rbnode *a, const struct rbnode *b)
{
    const struct timer *tima = (const struct timer *)a;
    const struct timer *timb = (const struct timer *)b;
    if (tima->trigger < timb->trigger) {
        return -1;
    } else if (tima->trigger == timb->trigger) {
        return 0;
    } else {
        return 1;
    }
}

static void run_timers(struct event_base *base)
{
    struct rbtree *tree = &base->timers;
    uint64_t now = get_time();
    while (1) {
        struct timer *t = (struct timer *)rbtree_leftmost(tree->root);
        if (!t) break;
        if (now >= t->trigger) {
            if (t->flags & TIMER_FLAG_ACTIVE) {
                // NOTE: This is in reverse order -- first delete/update the timer, and then call the callback
                // This is done, because the timer structure might be freed in the callback, and after that it
                // becomes inaccessible.
                if (t->flags & TIMER_FLAG_ONESHOT) {
                    del_timer(base, t);
                } else {
                    update_trigger(t);
                }

                assert(t->cb);
                t->cb(t->data);
            } else {
                del_timer(base, t);
            }

        } else {
            break;
        }
    }
}

static int run_io(struct event_base *base, int timeout)
{
    struct epoll_event events[MAX_IO_EVENTS];
    int n = epoll_wait(base->epollfd, events, MAX_IO_EVENTS, timeout);
    if (n) {
        for (int i = 0; i < n; i++) {
            struct event *event = events[i].data.ptr;
            assert(event->cb);
            event->cb(event->fd, events[i].events, event->data);
        }
    } else if (n < 0) {
        return ERR;
    }
    return OK;
}

struct event_base *create_event_base(void)
{
    struct event_base *base = calloc(1, sizeof *base);
    if (base) {
        rbtree_init(&base->timers, sizeof(struct timer), timer_cmp);
        int epollfd = epoll_create1(0);
        if (epollfd >= 0)
            base->epollfd = epollfd;
    }
    return base;
}

void destroy_event_base(struct event_base *base)
{
    if (base->epollfd >= 0)
        close(base->epollfd);
    free(base);
}

int event_base_iter(struct event_base *base)
{
    run_timers(base);
    return run_io(base, IO_TIMEOUT);
}

int add_event(struct event_base *base, struct event *event)
{
    struct epoll_event e;
    e.events = event->events;
    e.data.ptr = event;
    if (epoll_ctl(base->epollfd, EPOLL_CTL_ADD, event->fd, &e))
        return ERR;
    event->flags |= EVENT_FLAG_ACTIVE;
    return OK;
}

int del_event(struct event_base *base, struct event *event)
{
    // For really old kernels, this is necessary.
    // See man epoll_ctl, section "BUGS"
    struct epoll_event e = {0};
    if (epoll_ctl(base->epollfd, EPOLL_CTL_DEL, event->fd, &e))
        return ERR;
    event->flags &= ~EVENT_FLAG_ACTIVE;
    return OK;
}

int add_timer(struct event_base *base, struct timer *timer)
{
    if (timer->flags & TIMER_FLAG_ACTIVE)
        return ERR;

    update_trigger(timer);
    timer->flags |= TIMER_FLAG_ACTIVE;
    rbtree_insert(&base->timers, &timer->node);
    return OK;
}

int del_timer(struct event_base *base, struct timer *timer)
{
    if (timer->flags & TIMER_FLAG_ACTIVE) {
        rbtree_delete(&base->timers, &timer->node);
        timer->flags &= ~TIMER_FLAG_ACTIVE;
        return OK;
    }
    return ERR;
}
