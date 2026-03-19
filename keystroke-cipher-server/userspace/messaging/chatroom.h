#ifndef CHATROOM_H
#define CHATROOM_H

#define MAX_CHAT_MESSAGES 64

//Userspace msg struct
typedef struct {
    int id;
    char sender[64];
    long timestamp;
    char encrypted_preview[256];  
} chat_msg_t;
/*
 * chatroom_send - send an encrypted message to all peers via chatroom buffer
 * - write to /dev/keycipher_out (kernel encrypts)
 * - call client_broadcast(encrypted, is_chatroom=1)
 * - blocks if any peer's chatroom buffer is full (backpressure)
 */
int chatroom_send(const char *plaintext);

/*
 * chatroom_receive - called by server when a chatroom message arrives
 * - write encrypted message to chatroom FIFO in kernel
 * - if chatroom FIFO full: return -ENOBUFS (triggers HTTP 429 to sender)
 * - the counting semaphore in the kernel tracks available slots
 *   and this is what gets displayed as the semaphore counter in the UI
 */
int chatroom_receive(const char *encrypted_msg, const char *sender_ip);

/*
 * chatroom_read_loop - thread that drains chatroom FIFO when user reads
 * - blocking read from /dev/keycipher_in filtered for chatroom messages
 * - each read frees one slot: semaphore count increments
 * - blocked remote senders unblock when slot freed
 * - store decrypted message for frontend to display
 */
void *chatroom_read_loop(void *arg);

/*
 * chatroom_get_semaphore_count - return current free slots
 * - reads /proc/keycipher/stats
 * - returns chatroom_free value
 * - used by API to send live semaphore counter to frontend
 */
int chatroom_get_semaphore_count(void);



int chatroom_get_message_count(void);
chat_msg_t *chatroom_get_messages(void);

#endif /* CHATROOM_H */
