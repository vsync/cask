#include "http.h"

const kv_t g_http_methods[HTTP_METHOD_MAX] =
{
    make_kv("GET", 3),
    make_kv("HEAD", 4),
    make_kv("POST", 4),
    make_kv("PUT", 3),
    make_kv("DELETE", 6),
    make_kv("CONNECT", 7),
    make_kv("OPTIONS", 7),
    make_kv("TRACE", 5),
    make_kv("PATCH", 5)
};

const kv_t g_http_versions[HTTP_VERSION_MAX] =
{
    make_kv("HTTP/1.0", 8),
    make_kv("HTTP/1.1", 8),
    make_kv("HTTP/2.0", 8)
};

const kv_t g_http_statuses[HTTP_STATUS_MAX] =
{
    make_kv("200 OK", 6),
    make_kv("400 Bad Request", 15),
    make_kv("404 Not Found", 13),
    make_kv("500 Internal Server Error", 25)
};

const kv_t g_http_hkeys[HTTP_HKEY_MAX] =
{
    make_kv("Content-Length", 14),
    make_kv("Connection", 10),
    make_kv("Keep-Alive", 10)
};
