#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "direct.h"
#include "../network/client.h"

#define DEVICE_OUT "/dev/keycipher_out"
#define DEVICE_IN "/dev/keycipher_in"

//Kernel message struct 
typedef struct {
    long long tv_sec;
    long tv_nsec;
    char author[64];
    char data[256];
    int len;
} kernel_msg_t;

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
    return client_send_message(peer, (const char*)&encrypted, 0);
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
    }

    close(dev_fd);
    return NULL;
}
