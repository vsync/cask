#ifndef EVENT_H
#define EVENT_H

#include "common.h"
#include "list.h"
#include "rbtree.h"
#include <sys/epoll.h>

struct event_base;

typedef void (*event_callback)(int, uint32_t, void *);
typedef void (*timer_callback)(void *);

#define EVENT_FLAG_ACTIVE (1 << 0)

struct event
{
    int fd;
    uint32_t events;
    int flags;
    event_callback cb;
    void *data;
};

#define TIMER_FLAG_ACTIVE (1 << 0)
#define TIMER_FLAG_ONESHOT (1 << 1)

struct timer
{
    struct rbnode node;
    uint64_t trigger;
    uint32_t interval;
    int flags;
    timer_callback cb;
    void *data;
};

static inline struct event make_event(int fd, uint32_t events, event_callback cb, void *data)
{
    struct event event = {0};
    event.fd = fd;
    event.events = events;
    event.cb = cb;
    event.data = data;
    return event;
}

// NOTE: Interval is in milliseconds
static inline struct timer make_timer(uint32_t interval, int mode, timer_callback cb, void *data)
{
    struct timer timer = {0};
    timer.interval = interval;
    timer.flags = mode;
    timer.cb = cb;
    timer.data = data;
    return timer;
}

struct event_base *create_event_base(void);
void destroy_event_base(struct event_base *base);
int event_base_iter(struct event_base *base);
int add_event(struct event_base *base, struct event *event);
int del_event(struct event_base *base, struct event *event);
int add_timer(struct event_base *base, struct timer *timer);
int del_timer(struct event_base *base, struct timer *timer);

#endif
