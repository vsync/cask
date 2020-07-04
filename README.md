# cask
A pastebin server with a hash-table database, written in C

## Design:
- Libevent-style event system, implemented with epoll. Supports I/O events and timers, implemented with a red-black tree.
- Worker (thread) pool.
- NGINX-style socket sharding, all workers listen to the same address/port
- A hash-table database for storing the data (incomplete, WIP)
- An early draft of a monitor application for the server, communicates through a UNIX socket. Very, very WIP. Only worker and connection count is currently implemented.
