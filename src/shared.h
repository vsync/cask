#ifndef SHARED_H
#define SHARED_H

// Shared definitions between the server and the monitor

#include "common.h"

#define UNIX_PATH_MAX 108

#define IPC_CMD_STATUS 0x00

#pragma pack(push, 1)
struct ipc_request
{
    uint32_t command;
    uint32_t payload;
};

// NOTE: IPC responses are sent prefixed with a 32-bit command header,
// specifying the command being responded to.
struct ipc_status
{
    uint64_t uptime;
    uint32_t num_workers;
};

struct worker_status
{
    thread_id id;
    bool running;
    size_t num_conns;
};

#pragma pack(pop)

#endif
