#ifndef CLIENT_H
#define CLIENT_H

#include "peer_manager.h"

/*
 * client_connect - establish HTTPS connection to a single peer
 * - create SSL context
 * - TCP connect to peer->ip:peer->port
 * - SSL handshake
 * - update peer->status to PEER_CONNECTED on success
 * - update peer->status to PEER_DISCONNECTED on failure
 * returns 0 on success, -1 on failure
 */
int client_connect(peer_t *peer);

/*
 * client_send_message - send an encrypted message to a specific peer
 * - if peer->status != PEER_CONNECTED, return -1
 * - HTTP POST /message with encrypted body
 * - if response is HTTP 429 (peer buffer full):
 *     set peer->status = PEER_BLOCKED
 *     BLOCK until peer sends HTTP 200 (space available)
 *     this is where backpressure happens at userspace level
 * - if response is HTTP 200: message accepted
 * returns 0 on success, -1 on error
 */
int client_send_message(peer_t *peer, const char *encrypted_msg, size_t msg_len, int is_chatroom);

/*
 * client_broadcast - send a message to all connected peers
 * - iterate peers[]
 * - call client_send_message for each PEER_CONNECTED peer
 * - skip PEER_DISCONNECTED peers
 * - if any peer is PEER_BLOCKED, that peer's thread blocks independently
 *   so other peers still receive the message (non-blocking broadcast)
 */
void client_broadcast(const char *encrypted_msg, size_t msg_len, int is_chatroom);

/*
 * client_connect_thread - pthread entry point for peer_manager_connect_all
 * - casts arg to peer_t*
 * - calls client_connect in a loop with reconnect backoff
 */
void *client_connect_thread(void *arg);

/*
 * client_disconnect - cleanly close connection to a peer
 * - SSL_shutdown
 * - close socket
 * - set peer->status = PEER_DISCONNECTED
 */
void client_disconnect(peer_t *peer);

#endif /* CLIENT_H */
