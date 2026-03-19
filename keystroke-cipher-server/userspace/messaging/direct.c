#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "direct.h"
#include "../network/client.h"

#define DEVICE_OUT "/dev/keycipher_out"
#define DEVICE_IN "/dev/keycipher_in"

static user_msg_t inbox[MAX_MESSAGES];
static int inbox_count = 0;
static int next_id = 1;

int direct_get_message_count(void) {
    return inbox_count;
}

user_msg_t *direct_get_messages(void) {
    return inbox;
}

user_msg_t *direct_find_message_by_id(int id) {
    for (int i = 0; i < inbox_count; i++) {
        if (inbox[i].id == id)
            return &inbox[i];
    }
    return NULL;
}

/*
 * direct_send - write plaintext to /dev/keycipher_out
 * - open("/dev/keycipher_out", O_WRONLY)
 * - write(fd, plaintext, len) - kernel encrypts and puts in outgoing FIFO
 * - close(fd)
 * - client layer then reads outgoing FIFO and sends to peer
 */
int direct_send(peer_t *peer, const char *plaintext)
{
    kernel_msg_t msg;
    kernel_msg_t encrypted;
    int dev_fd;
    int ret;
    int bytes;

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.data, plaintext, sizeof(msg.data) - 1); // strncpy Limits how many bytes copied preventing overflowing the buffer
    msg.len = strlen(plaintext);

    dev_fd = open(DEVICE_OUT, O_WRONLY);
    if (dev_fd < 0) {
        perror("direct_send: Couldn't open device");
        return -1;
    }

    ret = write(dev_fd, &msg, sizeof(msg));
    close(dev_fd);
    if (ret < 0) {
        perror("direct_send: Write failure");
        return -1;
    }

    dev_fd = open(DEVICE_OUT, O_RDONLY);
    if (dev_fd < 0) {
        perror("direct_send: Couldn't read encrypted message");
        return -1;
    }

    bytes = read(dev_fd, &encrypted, sizeof(encrypted));
    close(dev_fd);
    if (bytes < 0) {
        perror("direct_send: Read encrypted failure");
        return -1;
    }

    //Hand over to client in network
    return client_send_message(peer, (const char*)&encrypted, sizeof(encrypted), 0);
}

/*
 * direct_send_loop - blocking read loop on /dev/keycipher_out
 * pops each encrypted outbox message and broadcasts it to all connected peers
 * blocks if outbox_fifo is empty (kernel semaphore)
 * if a peer's inbox is full, client_send_message handles the 429 backpressure retry
 */
void *direct_send_loop(void *arg)
{
    kernel_msg_t msg;
    int dev_fd;
    int bytes;

    dev_fd = open(DEVICE_OUT, O_RDONLY);
    if (dev_fd < 0) {
        perror("direct_send_loop: failed to open device");
        return NULL;
    }

    printf("direct_send_loop: started, waiting for outbox messages...\n");

    while (1) {
        bytes = read(dev_fd, &msg, sizeof(msg));
        if (bytes < 0) {
            perror("direct_send_loop: read failure");
            break;
        }
        if (bytes == 0) continue;

        printf("direct_send_loop: sending message from %s (%d chars)\n",
               msg.author, msg.len);
        client_broadcast((const char *)&msg, sizeof(msg), 0);
    }

    close(dev_fd);
    return NULL;
}

/*
 * direct_receive_loop - blocking read loop on /dev/keycipher_in
 * - open("/dev/keycipher_in", O_RDONLY)
 * - while(1): read(fd, buf, MAX_MESSAGE_LEN)
 *   - read BLOCKS if FIFO is empty (kernel semaphore)
 *   - kernel decrypts before returning data to userspace
 *   - store message in a userspace queue for API to serve to frontend
 */
void *direct_receive_loop(void *arg)
{
    kernel_msg_t msg;
    int dev_fd;
    int bytes;

    dev_fd = open(DEVICE_IN, O_RDONLY);
    if (dev_fd < 0) {
        perror("direct_receive_loop: Open device failure");
        return NULL;
    }

    printf("direct_receive_loop: Started, waiting for messages...\n");

    while (1) {
        bytes = read(dev_fd, &msg, sizeof(msg));
        if (bytes < 0) {
            perror("direct_receive_loop: Read message failure");
            break;
        }
        if (bytes == 0) continue;

        printf("%s: %.*s\n", msg.author, msg.len, msg.data);

        if (inbox_count < MAX_MESSAGES) {
            inbox[inbox_count].id = next_id++;
            strncpy(inbox[inbox_count].sender, msg.author, 63);
            inbox[inbox_count].timestamp = msg.tv_sec;
            strncpy(inbox[inbox_count].encrypted_preview, msg.data, 255);
            inbox_count++;
        }
    
    }

    close(dev_fd);
    return NULL;
}
