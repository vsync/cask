#ifndef ROUTE_H
#define ROUTE_H

#include "http.h"

struct request;
typedef struct buffer_t buffer_t;

enum route_match
{
    ROUTE_MATCH_EXACT,
    ROUTE_MATCH_PREFIX
};

typedef void (*route_callback)(struct request *req, void *data);

struct route
{
    enum http_method method;
    enum route_match match;
    const char *uri;
    size_t len;
    route_callback cb;
    void *data;
};

void add_route(enum http_method method, enum route_match match, const char *uri, route_callback cb, void *data);
const struct route *match_route(enum http_method method, const char *uri, size_t len);
void route_404(struct request *req);

#endif
