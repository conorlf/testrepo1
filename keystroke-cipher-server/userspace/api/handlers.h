#ifndef HANDLERS_H
#define HANDLERS_H

/*
 * All handlers receive a connected socket fd and write an HTTP JSON response.
 * The Node.js bridge calls these endpoints and forwards to the React frontend.
 *
 * GET  /api/stats           → buffer levels, peer statuses, semaphore count
 * GET  /api/messages        → list of encrypted messages in incoming FIFO
 * GET  /api/chatroom        → list of chatroom messages
 * POST /api/read/:id        → trigger ioctl read on one message (decrypt)
 * POST /api/read/all        → trigger KEYCIPHER_FLUSH_IN ioctl (READ ALL)
 * POST /api/send            → write message to /dev/keycipher_out
 * POST /api/send/chatroom   → write chatroom message
 */

void handle_get_stats(int client_fd);
void handle_get_messages(int client_fd);
void handle_get_outbox(int client_fd);
void handle_get_chatroom(int client_fd);
void handle_read_one(int client_fd, const char *message_id);
void handle_read_all(int client_fd);
void handle_send_direct(int client_fd, const char *body);
void handle_send_chatroom(int client_fd, const char *body);

#endif /* HANDLERS_H */
