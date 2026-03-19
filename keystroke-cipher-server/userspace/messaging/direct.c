#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "direct.h"
#include "../network/client.h"

#define DEVICE_OUT "/dev/keycipher_out"

/* inbox — filled by server.c when encrypted message arrives from peer */
static kernel_msg_t inbox[MAX_MESSAGES];
static int inbox_count = 0;

/* outbox — filled by direct_send_loop when message is popped and sent */
static kernel_msg_t outbox[MAX_MESSAGES];
static int outbox_waiting[MAX_MESSAGES]; /* 1 = blocked, 0 = sent */
static int outbox_count = 0;

/* inbox accessors */
int direct_get_inbox_count(void)  { return inbox_count; }
kernel_msg_t *direct_get_inbox(void) { return inbox; }

void direct_add_to_inbox(const kernel_msg_t *msg)
{
    if (inbox_count >= MAX_MESSAGES) return;
    inbox[inbox_count++] = *msg;
}

int direct_pop_inbox_front(void)
{
    if (inbox_count == 0) return -1;
    for (int i = 0; i < inbox_count - 1; i++)
        inbox[i] = inbox[i + 1];
    inbox_count--;
    return 0;
}

/* outbox accessors */
int direct_get_outbox_count(void)    { return outbox_count; }
kernel_msg_t *direct_get_outbox(void) { return outbox; }
int *direct_get_outbox_waiting(void)  { return outbox_waiting; }

/*
 * direct_send_loop - blocks on /dev/keycipher_out waiting for keyboard intercept
 * to push an encrypted message, then broadcasts to all peers
 */
void *direct_send_loop(void *arg)
{
    (void)arg;
    kernel_msg_t msg;
    int dev_fd;

    dev_fd = open(DEVICE_OUT, O_RDONLY);
    if (dev_fd < 0) {
        perror("direct_send_loop: failed to open device");
        return NULL;
    }

    printf("direct_send_loop: started, waiting for outbox messages...\n");

    while (1) {
        int bytes = read(dev_fd, &msg, sizeof(msg));
        if (bytes < 0) { perror("direct_send_loop: read failure"); break; }
        if (bytes == 0) continue;

        /* add to outbox as waiting */
        int idx = -1;
        if (outbox_count < MAX_MESSAGES) {
            idx = outbox_count;
            outbox[idx] = msg;
            outbox_waiting[idx] = 1;
            outbox_count++;
        }

        printf("direct_send_loop: broadcasting message from %s (%d chars)\n",
               msg.author, msg.len);

        /* client_broadcast blocks internally on 429 backpressure until delivered */
        client_broadcast((const char *)&msg, sizeof(msg), 0);

        /* mark sent */
        if (idx >= 0)
            outbox_waiting[idx] = 0;
    }

    close(dev_fd);
    return NULL;
}
