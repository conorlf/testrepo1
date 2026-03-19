#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "chatroom.h"
#include "../network/client.h"


static chat_msg_t chat_inbox[MAX_CHAT_MESSAGES];
static int chat_inbox_count = 0;
static int next_chat_id = 1;*/

int chatroom_get_message_count(void) {
    return chat_inbox_count;
}

chat_msg_t *chatroom_get_messages(void) {
    return chat_inbox;
}*/

#define DEVICE_OUT       "/dev/keycipher_out"
#define DEVICE_CHATROOM  "/dev/keycipher_chatroom"
#define PROC_STATS       "/proc/keycipher/stats"

typedef struct {
    long long tv_sec;
    long      tv_nsec;
    char      author[64];
    char      data[256];
    int       len;
    int       is_chatroom;
} kernel_msg_t;

/*
 * chatroom_send - encrypt via kernel then broadcast to all peers
 * - open /dev/keycipher_out, write plaintext
 *   kernel encrypts and places in outgoing FIFO
 * - client_broadcast reads outgoing FIFO and sends to all peers
 * - if a peer's chatroom FIFO is full they respond 429
 *   that peer's send thread blocks until space is available
 */
int chatroom_send(const char *plaintext)
{
    kernel_msg_t msg;
    kernel_msg_t encrypted;
    int dev_fd;
    int ret;
    int bytes;

    if (!plaintext) return -1;

    // build message 
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.data, plaintext, sizeof(msg.data) - 1);
    msg.len          = strlen(plaintext);
    msg.is_chatroom  = 1;

    // write to kernel - kernel encrypts it 
    dev_fd = open(DEVICE_OUT, O_WRONLY);
    if (dev_fd < 0) {
        perror("chatroom_send: Cannot open device");
        return -1;
    }

    ret = write(dev_fd, &msg, sizeof(msg));
    close(dev_fd);
    if (ret < 0) {
        perror("chatroom_send: Write failed");
        return -1;
    }

    // read encrypted result back 
    dev_fd = open(DEVICE_OUT, O_RDONLY);
    if (dev_fd < 0) {
        perror("chatroom_send: Cannot read encrypted");
        return -1;
    }

    bytes = read(dev_fd, &encrypted, sizeof(encrypted));
    close(dev_fd);
    if (bytes < 0) {
        perror("chatroom_send: Read encrypted failed");
        return -1;
    }

    // broadcast to ALL peers - 1 means chatroom message 
    client_broadcast((const char*)&encrypted, 1);
    return 0;
}

/*
 * chatroom_receive - incoming chatroom message from a remote peer
 * - check chatroom_fifo has space via fifo_is_full check on /proc/stats
 * - if full: return -ENOBUFS so server.c can respond HTTP 429
 * - if space: open /dev/keycipher_in, write encrypted message
 *   this decrements the chatroom semaphore (one less free slot)
 * - return 0 on success
 */
int chatroom_receive(const char *encrypted_msg, const char *sender_ip)
{
    int dev_fd;
    int ret;

    //returns immediately if full instead of blocking
    dev_fd = open(DEVICE_CHATROOM, O_WRONLY | O_NONBLOCK);
    if (dev_fd < 0) {
        perror("chatroom_receive: Cannot open device");
        return -1;
    }

    ret = write(dev_fd, encrypted_msg, sizeof(kernel_msg_t));
    close(dev_fd);

    if (ret < 0) {
        printf("chatroom_receive: FIFO full from %s\n", sender_ip);
        return -ENOBUFS;
    }

    printf("chatroom_receive: Message stored from %s\n", sender_ip);
    return 0;
}

/*
 * chatroom_read_loop - continuously drain chatroom FIFO on user READ
 * - this thread is BLOCKED by the kernel until messages are available
 * - when user clicks READ or READ ALL in frontend:
 *   - ioctl KEYCIPHER_FLUSH_IN or individual read is triggered
 *   - kernel decrypts and returns plaintext
 *   - semaphore count increases (slot freed)
 *   - any remote peer that was blocked sending to us will now unblock
 * - store decrypted message in userspace list for API to serve
 */
void *chatroom_read_loop(void *arg)
{
    kernel_msg_t msg;
    int dev_fd;
    int bytes;

    dev_fd = open(DEVICE_CHATROOM, O_RDONLY);
    if (dev_fd < 0) {
        perror("chatroom_read_loop: Cannot open device");
        return NULL;
    }

    printf("chatroom_read_loop: Started, waiting for messages\n");
    while (1) {
        /* blocks here until chatroom FIFO has a message
           kernel wakes this thread via semaphore up()
           kernel decrypts before returning               */
           bytes = read(dev_fd, &msg, sizeof(msg));
           if (bytes < 0) {
            perror("chatroom_read_loop: Read error");
            break;
           }
           if (bytes == 0) continue;

           //slot freed
           printf("CHATROOM (%s): %.*s\n", msg.author, msg.len, msg.data);
    }
    
    close(dev_fd);
    return NULL;
}

/*
 * chatroom_get_semaphore_count - parse /proc/keycipher/stats
 * - open and read /proc/keycipher/stats
 * - find chatroom_free line
 * - parse and return integer value
 * - called every 500ms by API to push live count to frontend
 */
int chatroom_get_semaphore_count(void)
{
    FILE *f;
    char line[128];
    int count = 0;

    f = fopen(PROC_STATS, "r");
    if (!f) {
        perror("chatroom_get_semaphore_count: Cannot open proc stats");
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "chatroom_free", 13) == 0) {
            sscanf(line, "chatroom_free: %d", &count);
            break;
        }
    }

    fclose(f);
    return count;
}
