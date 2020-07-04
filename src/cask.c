// TODO:
// - Routes are not freed upon exit, and should probably be put to the cask structure.
// - Better "Not found" handling in connection.c
// - The server-monitor IPC socket could be UDP instead of TCP.
// - File monitoring? (inotify?)

#include "cask.h"
#include "shared.h"
#include "db.h"
#include "buffer.h"
#include "event.h"
#include "list.h"
#include "request.h"
#include "route.h"
#include "worker.h"
#include "file.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define NUM_WORKERS 3
#define HOST NULL
#define PORT "3000"

#define DB_PATH "cask.db"
#define DB_NUM_BUCKETS 100000

#define IPC_SOCK_PATH "cask.sock"

#define INDEX_PATH "index.html"

static struct cask cask;
struct cask *g_cask;

static void sighandler(int sig)
{
    UNUSED(sig);
    fprintf(stderr, "Exiting...\n");
    cask.running = false;
}

static int setup_ipc(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("setup_ipc: socket");
        return ERR;
    }
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path)); // NOLINT [C11 Annex K]
    if (bind(fd, &addr, sizeof(addr)) < 0) {
        perror("setup_ipc: bind");
        close(fd);
        return -1;
    }
    if (listen(fd, 10) < 0) {
        perror("setup_ipc: listen");
        close(fd);
        return -1;
    }
    return fd;
}

static void handle_ipc(void)
{
    int fd = accept(cask.ipcfd, NULL, NULL);
    if (fd < 0) {
        perror("handle_ipc: accept");
        return;
    }

    struct ipc_request req;
    if (read(fd, &req, sizeof req) != sizeof req) {
        // TODO: Make this more robust
        // TODO: Probably could use a nonblocking socket here too.
        // Invalid request (this may not be strictly true)
        return;
    }

    switch (req.command) {
        case IPC_CMD_STATUS: {
            uint64_t uptime = (get_time() - cask.started_at) / 1000000000ULL;
            pthread_mutex_lock(&cask.worker_lock);

            uint32_t command = IPC_CMD_STATUS;
            struct ipc_status status;
            status.uptime = uptime;
            status.num_workers = cask.num_workers;
            write(fd, &command, sizeof command);
            write(fd, &status, sizeof status);

            struct list *iter = LIST_HEAD(&cask.workers);
            for (uint32_t i = 0; i < status.num_workers; i++) {
                struct worker *worker = list_entry(iter, struct worker, node);
                struct worker_status worker_status;
                worker_status.id = worker->id;
                worker_status.running = worker->running;
                worker_status.num_conns = worker->num_conns;
                write(fd, &worker_status, sizeof worker_status);
                iter = iter->next;
            }

            pthread_mutex_unlock(&cask.worker_lock);
        } break;
    }
    close(fd);
}

static void index_callback(struct request *req, void *data)
{
    struct file *file = data;
    if (file->size) {
        send_response(req, HTTP_STATUS_200, file->data, file->size);
    } else {
        static const char resp[] = "Index.";
        send_response(req, HTTP_STATUS_200, resp, strlen(resp));
    }
}

static void get_callback(struct request *req, void *data)
{
    struct db *db = data;
    int len;
    const char *uri = get_uri(req, &len);

    // Validate URI
    // Skip the leading '/'
    for (int i = 1; i < len; i++) {
        if (!isdigit(uri[i])) {
            // If the URI has a valid id, followed with a trailing slash, i.e. "/id/",
            // it's still a valid URI. However, "//" should fail validation.
            if ((uri[i] == '/') && (len > 2) && (i == len-1))
                break;

            send_response(req, HTTP_STATUS_404, NULL, 0);
            return;
        }
    }

    dbid_t id = strtoull(uri+1, NULL, 10);
    uint32_t entry_len;
    void *entry = db_get(db, id, &entry_len);
    if (entry) {
        send_response(req, HTTP_STATUS_200, entry, entry_len);
        free(entry);
    } else {
        static const char resp[] = "Paste not found";
        send_response(req, HTTP_STATUS_404, resp, strlen(resp));
    }
}

static void post_callback(struct request *req, void *data)
{
    struct db *db = data;
    int len;
    const char *body = get_body(req, &len);
    if (len == 0) {
        static const char resp[] = "Error: Zero length paste";
        send_response(req, HTTP_STATUS_400, resp, strlen(resp));
    } else {
        dbid_t id;
        if (db_insert(db, body, (uint32_t)len, &id) != OK) {
            send_response(req, HTTP_STATUS_500, NULL, 0);
        } else {
            char tmp[20];
            len = snprintf(tmp, 20, "%lu", id); // NOLINT [C11 Annex K]
            send_response(req, HTTP_STATUS_200, tmp, (size_t)len);
        }
    }
}

int main(int argc, char *argv[])
{
    g_cask = &cask;
    cask.started_at = get_time();

    LIST_INIT_HEAD(cask.workers);

    const char *port = PORT;
    uint32_t num_workers = NUM_WORKERS;
    const char *db_path = DB_PATH;
    size_t num_buckets = DB_NUM_BUCKETS;
    const char *ipc_sock_path = IPC_SOCK_PATH;
    int opt;
    while ((opt = getopt(argc, argv, "hp:w:d:b:s:")) != -1) {
        switch (opt) {
            case 'p': {
                for (size_t i = 0; i < strlen(optarg); i++) {
                    if (!isdigit(optarg[i])) {
                        fprintf(stderr, "Invalid port\n");
                        return 1;
                    }
                }
                port = optarg;
            } break;
            case 'w': {
                uint64_t n = strtoul(optarg, NULL, 10);
                if (n > UINT32_MAX || n == 0) {
                    fprintf(stderr, "Invalid worker count");
                    return 1;
                }
                num_workers = (uint32_t)n;
            } break;

            case 'd': {
                db_path = optarg;
            } break;

            case 'b': {
                num_buckets = strtoull(optarg, NULL, 10);
                if (num_buckets == 0) {
                    fprintf(stderr, "Number of database hash table buckets must be > 0\n");
                    return 1;
                }
            } break;

            case 's': {
                if (strlen(optarg) > UNIX_PATH_MAX) {
                    fprintf(stderr, "Sock path must be %d characters or less\n", UNIX_PATH_MAX);
                    return 1;
                }
                ipc_sock_path = optarg;
            } break;

            case 'h': {
                fprintf(stderr, "Usage:\n\n"
                    "  %s [options]\n\n"
                    " Server:\n\n"
                    "  -p PORT\tPort\n\n"
                    "Threading:\n\n"
                    "  -w WORKERS\tNumber of worker threads\n\n"
                    "Database:\n\n"
                    "  -d DBPATH\tDatabase file path\n"
                    "  -b BUCKETS\tNumber of database hash table buckets\n\n"
                    "Misc:\n\n"
                    "  -s SOCKPATH\tSocket path used for IPC listen\n\n",
                    argv[0]);
                return 1;
            }

            default: {
                return 1;
            }
        }
    }

    // Worker list mutex
    // TODO: Clean up if we fail.
    if (pthread_mutex_init(&cask.worker_lock, NULL)) {
        perror("Main: pthread_mutex_init");
        return 1;
    }

    // Signal handler
    struct sigaction sa = {0};
    sa.sa_handler = sighandler;
    sigaction(SIGINT, &sa, NULL);

    // Block SIGINT
    sigset_t mask, orig_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
        perror("Main: sigprogmask");
        return 1;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(HOST, port, &hints, &cask.ai);
    if (status) {
        perror("Main: getaddrinfo");
        return 1;
    }

    int exit_code = 0;

    // Database
    struct db_params params;
    params.num_buckets = num_buckets;
    struct db *db = open_db(db_path, &params);
    if (!db) {
        fprintf(stderr, "[FATAL] Main: open_db\n");
        freeaddrinfo(cask.ai);
        return 1;
    }

    // IPC socket
    cask.ipcfd = setup_ipc(ipc_sock_path);
    if (cask.ipcfd < 0) {
        close_db(db);
        freeaddrinfo(cask.ai);
        return 1;
    }

    // Routes
    struct file file = map_file(INDEX_PATH);
    add_route(HTTP_METHOD_GET, ROUTE_MATCH_EXACT, "/", index_callback, &file);
    add_route(HTTP_METHOD_GET, ROUTE_MATCH_PREFIX, "/", get_callback, db);
    add_route(HTTP_METHOD_POST, ROUTE_MATCH_EXACT, "/", post_callback, db);

    // Worker startup
    for (thread_id i = 0; i < num_workers; i++) {
        if (start_worker() != OK) {
            fprintf(stderr, "[FATAL] Main: create_worker\n");
            exit_code = 1;
            goto out;
        }
    }

    // Main monitor loop
    cask.running = true;
    while (cask.running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(cask.ipcfd, &fds);
        int res = pselect(cask.ipcfd+1, &fds, NULL, NULL, NULL, &orig_mask);
        if (res < 0 && errno != EINTR) {
            perror("Main: pselect");
            exit_code = 1;
            break;
        } else if (!cask.running) {
            exit_code = 1;
            break;
        } else if (res == 0) {
            continue;
        }

        if (FD_ISSET(cask.ipcfd, &fds))
            handle_ipc();
    }

out:;
    freeaddrinfo(cask.ai);
    pthread_mutex_destroy(&cask.worker_lock);
    close(cask.ipcfd);
    unlink(ipc_sock_path);
    unmap_file(&file);
    close_db(db);
    struct list *iter, *next;
    list_for_each_safe(iter, next, &cask.workers) {
        struct worker *worker = list_entry(iter, struct worker, node);
        shutdown_worker(worker);
    }

    return exit_code;
}
