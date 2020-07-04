#include "buffer.h"
#include "connection.h"
#include "request.h"
#include <stdio.h>
#include <stdlib.h>

static inline int span(const char *s, char c, int len, int off)
{
    const char *p = s+off;
    for (int i = 0; off < len; off++, i++, p++) {
        if (*p == c)
            return i+1;
    }
    return -1;
}

static inline int split(const char *s, int len, int off, char delim, string_t *tokens, int num_tokens)
{
    if (num_tokens == 0) return 0;
    if (num_tokens == 1) {
        tokens[0].off = off;
        tokens[0].len = len;
        return 1;
    }

    int start = 0;
    int i, stop;
    for (i = stop = 0; stop < len && i < num_tokens-1; stop++) {
        if (s[stop] == delim) {
            tokens[i].off = start + off;
            tokens[i].len = stop-start;
            start = stop+1;
            i++;
        }
    }
    tokens[i].off = start + off;
    tokens[i].len = len-start;
    return i+1;
}

static inline void trim(const char *buf, string_t *s)
{
    // Forwards
    int len = s->len;
    const char *c = buf + s->off;
    for (int i = 0; i < len; i++, c++) {
        if (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') {
            s->off++;
            s->len--;
        } else {
            break;
        }
    }

    // Backwards
    c = buf + s->off + s->len-1;
    len = s->len;
    for (int i = len; i > 0; i--, c--) {
        if (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') {
            s->len--;
        } else {
            break;
        }
    }
}

static int parse_status_line(struct request *req, const buffer_t *buf)
{
    const char *data = buf->data;
    int line_len = span(data, '\n', (int)buf->size, 0);
    if (line_len < 0) return REQ_UF;

    string_t tokens[3];
    if (split(data, line_len, 0, ' ', tokens, 3) != 3)
        return REQ_ERR;

    // Request method
    enum http_method method = HTTP_METHOD_UNKNOWN;
    for (int i = 0; i < HTTP_METHOD_MAX; i++) {
        if (g_http_methods[i].len == tokens[0].len) {
            if (strncmp(g_http_methods[i].s, data + tokens[0].off, (size_t)tokens[0].len) == 0) {
                method = (enum http_method)i;
                break;
            }
        }
    }
    if (method == HTTP_METHOD_UNKNOWN)
        return REQ_ERR;

    // HTTP Version
    trim(data, &tokens[2]);
    enum http_version version = HTTP_VERSION_UNKNOWN;
    for (int i = 0; i < HTTP_VERSION_UNKNOWN; i++) {
        if (g_http_versions[i].len == tokens[2].len) {
            if (strncmp(g_http_versions[i].s, data + tokens[2].off, (size_t)tokens[2].len) == 0) {
                version = (enum http_version)i;
                break;
            }
        }
    }
    if (version == HTTP_VERSION_UNKNOWN)
        return REQ_ERR;

    req->method = method;
    req->version = version;
    req->uri = tokens[1];
    req->read_bytes = (size_t)line_len;
    return REQ_OK;
}

static int parse_headers(struct request *req, const buffer_t *buf)
{
    const char *data = buf->data;
    for (; req->num_headers < MAX_HEADERS; req->num_headers++) {
        if (*(data + req->read_bytes) == '\r' && *(data + req->read_bytes + 1) == '\n') {
            req->read_bytes += 2;
            break;
        }

        int line_len = span(data, '\n', (int)buf->size, (int)req->read_bytes);
        if (line_len < 0) return REQ_UF;

        string_t tokens[2];
        if (split(data + req->read_bytes, line_len, (int)req->read_bytes, ':', tokens, 2) != 2)
            return REQ_ERR;
        trim(data, &tokens[0]);
        trim(data, &tokens[1]);

        struct http_header *header = &req->headers[req->num_headers];
        header->key = tokens[0];
        header->val = tokens[1];

        req->read_bytes += (size_t)line_len;
    }

    return REQ_OK;
}

static const struct http_header *find_header(const struct request *req, const char *key, int len, const buffer_t *buf)
{
    const char *data = buf->data;
    for (int i = 0; i < req->num_headers; i++) {
        const struct http_header *header = &req->headers[i];
        if (header->key.len == len) {
            if (strncmp(data + header->key.off, key, (size_t)len) == 0)
                return header;
        }
    }
    return NULL;
}

static inline struct connection *get_connection(struct request *req)
{
    return container_of(req, struct connection, req);
}

int parse_request(struct request *req)
{
    struct connection *c = get_connection(req);
    buffer_t *buf = c->buffer;

    switch (req->state) {
        case REQUEST_STATE_INIT: {
            int ret = parse_status_line(req, buf);
            if (ret == REQ_ERR || ret == REQ_UF) {
                return ret;
            } else {
                req->state = REQUEST_STATE_HEADERS;
            }
        }

        FALLTHROUGH;

        case REQUEST_STATE_HEADERS: {
            int ret = parse_headers(req, buf);
            if (ret == REQ_ERR || ret == REQ_UF) {
                return ret;
            } else {
                const struct http_header *cl = find_header(req, "Content-Length", (int)strlen("Content-Length"), buf);
                if (cl) {
                    int len = (int)strtol(buf->data + cl->val.off, NULL, 10);
                    if (len > MAX_BODY) {
                        return REQ_ERR;
                    } else {
                        req->body.off = (int)req->read_bytes;
                        req->body.len = len;
                    }
                }

                req->state = REQUEST_STATE_BODY;
            }
        }

        FALLTHROUGH;

        case REQUEST_STATE_BODY: {
            if (req->body.len == 0) {
                req->state = REQUEST_STATE_READ_COMPLETE;
            } else {
                req->read_bytes = buf->size;
                if ((int)req->read_bytes - req->body.off < req->body.len) {
                    return REQ_UF;
                } else {
                    req->state = REQUEST_STATE_READ_COMPLETE;
                }
            }
        }

        FALLTHROUGH;

        case REQUEST_STATE_READ_COMPLETE: {
            if (req->version == HTTP_VERSION_11)
                c->flags |= CONNECTION_FLAG_KEEPALIVE;
        } break;
    }
    return REQ_OK;
}

void send_response(struct request *req, enum http_status status, const char *body, size_t len)
{
    struct connection *c = get_connection(req);
    buffer_t *buf = c->buffer;
    clear_buffer(buf);

    char tmp[128];
    int n;

    n = snprintf(tmp, 128, "%s %s\r\n", g_http_versions[req->version].s, g_http_statuses[status].s); // NOLINT [C11 Annex K]
    push_buffer(buf, tmp, (size_t)n);

    if (c->flags & CONNECTION_FLAG_KEEPALIVE) {
        n = snprintf(tmp, 128, "%s: keep-alive\r\n", g_http_hkeys[HTTP_HKEY_CONNECTION].s); // NOLINT [C11 Annex K]
        push_buffer(buf, tmp, (size_t)n);
        n = snprintf(tmp, 128, "%s: timeout=5\r\n", g_http_hkeys[HTTP_HKEY_KEEP_ALIVE].s); // NOLINT [C11 Annex K]
        push_buffer(buf, tmp, (size_t)n);
    }

    n = snprintf(tmp, 128, "%s: %lu\r\n\r\n", g_http_hkeys[HTTP_HKEY_CONTENT_LENGTH].s, len); // NOLINT [C11 Annex K]
    push_buffer(buf, tmp, (size_t)n);

    
    if (len) {
        assert(body);
        push_buffer(buf, body, (size_t)len);
    }

    begin_send(c);
}

const char *get_uri(struct request *req, int *len)
{
    struct connection *c = get_connection(req);
    buffer_t *buf = c->buffer;
    *len = req->uri.len;
    return buf->data + req->uri.off;
}

const char *get_body(struct request *req, int *len)
{
    struct connection *c = get_connection(req);
    buffer_t *buf = c->buffer;
    *len = req->body.len;
    return buf->data + req->body.off;
}
