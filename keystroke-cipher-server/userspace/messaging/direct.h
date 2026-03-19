#ifndef DIRECT_H
#define DIRECT_H

#include "../network/peer_manager.h"

/*
 * direct_send - send an encrypted direct message to one peer
 * - read raw text from /dev/keycipher_out (already encrypted by kernel)
 * - call client_send_message(peer, encrypted, 0)
 * - update outbox in stats
 */
int direct_send(peer_t *peer, const char *plaintext);

/*
 * direct_receive_loop - thread that reads /dev/keycipher_in continuously
 * - blocking read from /dev/keycipher_in
 * - each read returns one decrypted message (decrypted by kernel on READ)
 * - notify API layer so frontend can poll and display it
 */
void *direct_receive_loop(void *arg);

//Kernel message struct 
typedef struct {
    long long tv_sec;
    long tv_nsec;
    char author[64];
    char data[256];
    int len;
} kernel_msg_t;

//Userspace message struct
typedef struct {
    int id;
    char sender[64];
    long timestamp;
    char encrypted_preview[256];
} user_msg_t;

int direct_get_message_count(void);
user_msg_t *direct_get_messages(void);

#endif /* DIRECT_H */
