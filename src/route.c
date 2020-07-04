#include "array.h"
#include "request.h"
#include "route.h"

static struct route *routes;

void add_route(enum http_method method, enum route_match match, const char *uri, route_callback cb, void *data)
{
    struct route route;
    route.method = method;
    route.match = match;
    route.uri = uri;
    route.len = strlen(uri);
    route.cb = cb;
    route.data = data;
    array_push_back(routes, route);
}

const struct route *match_route(enum http_method method, const char *uri, size_t len)
{
    const struct route *match = NULL;
    size_t longest = 0;
    for (size_t i = 0; i < array_size(routes); i++) {
        const struct route *route = &routes[i];
        if (route->method != method)
            continue;

        if (route->match == ROUTE_MATCH_EXACT) {
            if (route->len == len) {
                if (strncmp(route->uri, uri, route->len) == 0)
                    return route;
            }
        } else {
            if ((strncmp(route->uri, uri, route->len) == 0) && route->len > longest) {
                match = route;
                longest = route->len;
            }
        }
    }
    return match;
}

void route_404(struct request *req)
{
    send_response(req, HTTP_STATUS_404, NULL, 0);
}
