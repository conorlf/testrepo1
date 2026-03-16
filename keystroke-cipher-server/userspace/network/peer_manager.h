#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#define MAX_PEERS 16

typedef enum {
    PEER_CONNECTED,
    PEER_BLOCKED, //if their incoming FIFO is full; waiting
    PEER_OFFLINE
} peer_status_t;

typedef struct {
    char          ip[64];
    int           port;
    peer_status_t status;
    int           socket_fd;
} peer_t;

int     peer_manager_init(const char *config_file);
peer_t *peer_get_all(int *count);
void    peer_set_status(const char *ip, peer_status_t status);
void    peer_manager_cleanup(void);

#endif
