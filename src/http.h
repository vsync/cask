#ifndef HTTP_H
#define HTTP_H

#include "common.h"

#define make_kv(s, len) (kv_t){(s), (len)}

// TODO: Rename this to something more to the point...
typedef struct kv_t
{
    const char *s;
    int len;
} kv_t;

// TODO: Rename this to string reference or something?
typedef struct string_t
{
    int off;
    int len;
} string_t;

enum http_method
{
    HTTP_METHOD_GET,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_UNKNOWN
};
#define HTTP_METHOD_MAX HTTP_METHOD_UNKNOWN

extern const kv_t g_http_methods[HTTP_METHOD_MAX];

enum http_version
{
    HTTP_VERSION_10,
    HTTP_VERSION_11,
    HTTP_VERSION_20,
    HTTP_VERSION_UNKNOWN
};
#define HTTP_VERSION_MAX HTTP_VERSION_UNKNOWN

extern const kv_t g_http_versions[HTTP_VERSION_MAX];

enum http_status
{
    HTTP_STATUS_200,
    HTTP_STATUS_400,
    HTTP_STATUS_404,
    HTTP_STATUS_500,
    HTTP_STATUS_UNKNOWN
};
#define HTTP_STATUS_MAX HTTP_STATUS_UNKNOWN

extern const kv_t g_http_statuses[HTTP_STATUS_MAX];

struct http_header
{
    string_t key;
    string_t val;
};

enum http_hkey
{
    HTTP_HKEY_CONTENT_LENGTH,
    HTTP_HKEY_CONNECTION,
    HTTP_HKEY_KEEP_ALIVE,
    HTTP_HKEY_UNKNOWN
};
#define HTTP_HKEY_MAX HTTP_HKEY_UNKNOWN

extern const kv_t g_http_hkeys[HTTP_HKEY_MAX];

#endif
