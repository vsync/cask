#ifndef REQUEST_H
#define REQUEST_H

#include "http.h"

#define MAX_HEADERS 32
#define MAX_BODY (128*1024)

struct connection;

enum request_state
{
    REQUEST_STATE_INIT,
    REQUEST_STATE_HEADERS,
    REQUEST_STATE_BODY,
    REQUEST_STATE_READ_COMPLETE
};

struct request
{
    enum request_state state;

    size_t read_bytes;

    enum http_method method;
    enum http_version version;
    string_t uri;
    int num_headers;
    struct http_header headers[MAX_HEADERS];
    string_t body;
};

#define REQ_ERR (-1)
#define REQ_OK (0)
#define REQ_UF (1)

int parse_request(struct request *req);
void send_response(struct request *req, enum http_status status, const char *body, size_t len);

const char *get_uri(struct request *req, int *len);
const char *get_body(struct request *req, int *len);

// TODO: API to get the headers, too

#endif
