#include "shared.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h': {
                fprintf(stderr, "Usage:\n\n"
                    "  %s socket-path\n\n", argv[0]);
                return 1;
            }

            default: {
                return 1;
            }
        }
    }

    if (argc < 2) {
        fprintf(stderr, "A socket path must be provided\n");
        return 1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Main: socket");
        return 1;
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, argv[1], strlen(argv[1])); // NOLINT [C11 Annex K]
    if (connect(fd, &addr, sizeof(addr)) < 0) {
        perror("Main: connect");
        close(fd);
        return 1;
    }

    struct ipc_request req = {IPC_CMD_STATUS, 0};
    if (write(fd, &req, sizeof req) != sizeof req) {
        perror("Main: write");
        close(fd);
        return 1;
    }

    // Read command
    uint32_t command;
    if (read(fd, &command, sizeof command) != sizeof command) {
        perror("Main: read");
        close(fd);
        return 1;
    }
    if (command != IPC_CMD_STATUS) {
        fprintf(stderr, "IPC_CMD_STATUS: Invalid response from server: %u\n", command);
        close(fd);
        return 1;
    }

    // Read status header
    struct ipc_status status;
    if (read(fd, &status, sizeof status) != sizeof status) {
        perror("Main: read");
        close(fd);
        return 1;
    }

    fprintf(stderr, "Status: \n"
        "Uptime: %lu seconds\n"
        "Number of workers: %u\n",
        status.uptime, status.num_workers);

    // Read payload
    for (uint32_t i = 0; i < status.num_workers; i++) {
        struct worker_status worker_status;
        if (read(fd, &worker_status, sizeof worker_status) != sizeof worker_status) {
            perror("Main: read");
            close(fd);
            return 1;
        }

        fprintf(stderr, "  Worker #%lu:\n"
            "  Status: %s\n"
            "  Number of connections: %lu\n\n",
            worker_status.id, worker_status.running ? "Running" : "Terminated",
            worker_status.num_conns);
    }

    close(fd);

    return 0;
}
