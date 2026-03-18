#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "chatroom.h"
#include "../network/client.h"

#define DEVICE_OUT       "/dev/keycipher_out"
#define DEVICE_CHATROOM  "/dev/keycipher_chatroom"
#define PROC_STATS       "/proc/keycipher/stats"

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
    /* TODO: implement chatroom broadcast */
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
    /* TODO: implement chatroom message receive */
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
    /* TODO: implement chatroom read loop */
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
    /* TODO: implement proc stats parsing */
    return 0;
}
