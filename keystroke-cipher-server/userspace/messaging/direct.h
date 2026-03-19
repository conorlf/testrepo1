#ifndef DIRECT_H
#define DIRECT_H

#include "../network/peer_manager.h"

#define MAX_MESSAGES 64

/* single message struct used everywhere — matches kernel/keycipher.h layout */
typedef struct {
    long long tv_sec;
    long      tv_nsec;
    char      author[64];
    char      data[256];
    int       len;
} kernel_msg_t;

/* send loop thread — drains /dev/keycipher_out and broadcasts to peers */
void *direct_send_loop(void *arg);

/* inbox — populated by server.c when a message arrives */
void          direct_add_to_inbox(const kernel_msg_t *msg);
int           direct_pop_inbox_front(void);
int           direct_get_inbox_count(void);
kernel_msg_t *direct_get_inbox(void);

/* outbox — populated by direct_send_loop when a message is sent */
int           direct_get_outbox_count(void);
kernel_msg_t *direct_get_outbox(void);
int          *direct_get_outbox_waiting(void);

#endif /* DIRECT_H */
