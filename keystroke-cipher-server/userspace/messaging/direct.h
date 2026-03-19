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

/*int direct_get_message_count(void);
user_msg_t *direct_get_messages(void);*/

#endif /* DIRECT_H */
