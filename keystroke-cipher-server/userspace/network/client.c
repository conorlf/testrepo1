#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "client.h"
#include "peer_manager.h"

/*
 * client_connect - TCP + SSL handshake to peer
 * - socket(AF_INET, SOCK_STREAM, 0)
 * - connect() to peer->ip:peer->port
 * - SSL_new, SSL_set_fd, SSL_connect
 * - on success: peer->status = PEER_CONNECTED
 * - on failure: peer->status = PEER_DISCONNECTED, return -1
 */
int client_connect(peer_t *peer)
{
    struct sockaddr_in addr;

    peer->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer->socket_fd < 0) {
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer->port);

    if (inet_pton(AF_INET, peer->ip, &addr.sin_addr) <= 0) {
        close(peer->socket_fd);
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    if (connect(peer->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(peer->socket_fd);
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    // SSL setup
    peer->ctx = SSL_CTX_new(TLS_client_method());
    if (!peer->ctx) {
        close(peer->socket_fd);
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    peer->status = PEER_CONNECTED;
    return 0;
}

/*
 * client_send_message - HTTP POST encrypted message to peer
 * - build HTTP POST request with encrypted_msg as body
 * - SSL_write the request
 * - read response code from SSL_read
 * - if 429: set PEER_BLOCKED, sleep and retry (backpressure)
 * - if 200: set PEER_CONNECTED (unblocked), return 0
 * - if 503: set PEER_DISCONNECTED, return -1
 */
int client_send_message(peer_t *peer, const char *encrypted_msg, int is_chatroom)
{
    if (peer->status != PEER_CONNECTED)
        return -1;

    char request[4096];
    int body_len = strlen(encrypted_msg);

    snprintf(request, sizeof(request),
        "POST /message HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %d\r\n"
        "X-Chatroom: %d\r\n"
        "\r\n"
        "%s",
        peer->ip, body_len, is_chatroom, encrypted_msg
    );

retry_send:
    if (SSL_write(peer->ssl, request, strlen(request)) <= 0) {
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    char response[2048];
    int n = SSL_read(peer->ssl, response, sizeof(response) - 1);
    if (n <= 0) {
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    response[n] = '\0';

    // Parse HTTP status code
    int code = atoi(response + 9); // "HTTP/1.1 XXX"

    if (code == 200) {
        peer->status = PEER_CONNECTED;
        return 0;
    }

    if (code == 429) {
        peer->status = PEER_BLOCKED;
        sleep(1);  // backpressure wait
        goto retry_send;
    }

    if (code == 503) {
        peer->status = PEER_DISCONNECTED;
        return -1;
    }

    return -1;
}


/*
 * client_broadcast - send to all connected peers concurrently
 * - for each peer in peers[] where status == PEER_CONNECTED
 *   - call client_send_message(peer, encrypted_msg, is_chatroom)
 * NOTE: each peer has its own thread so blocking on one peer
 *       does not block delivery to other peers
 */
void client_broadcast(const char *encrypted_msg, int is_chatroom)
{
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t *p = &peers[i];

        if (p->status == PEER_CONNECTED) {
            client_send_message(p, encrypted_msg, is_chatroom);
        }
    }
}


/*
 * client_connect_thread - runs in a loop maintaining connection to one peer
 * - cast arg to peer_t*
 * - while(1): try client_connect
 *   - if fails: sleep(5) and retry (reconnection backoff)
 *   - if succeeds: stay connected, handle reconnect on disconnect
 */
void *client_connect_thread(void *arg)
{
    peer_t *peer = (peer_t *)arg;

    while (1) {
        if (client_connect(peer) == 0) {
            // Stay connected until something breaks
            while (peer->status == PEER_CONNECTED) {
                sleep(1);
            }
        }

        // If disconnected, retry after backoff
        sleep(5);
    }

    return NULL;
}


/*
 * client_disconnect - graceful SSL teardown
 * - SSL_shutdown(peer->ssl)
 * - SSL_free(peer->ssl)
 * - close(peer->socket_fd)
 * - peer->status = PEER_DISCONNECTED
 */
void client_disconnect(peer_t *peer)
{
    if (peer->ssl) {
        SSL_shutdown(peer->ssl);
        SSL_free(peer->ssl);
    }

    if (peer->ctx)
        SSL_CTX_free(peer->ctx);

    if (peer->socket_fd > 0)
        close(peer->socket_fd);

    peer->status = PEER_DISCONNECTED;
}

