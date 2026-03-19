#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "handlers.h"
#include "../messaging/direct.h"
#include "../network/peer_manager.h"

#define KEYCIPHER_MAGIC   'K'
#define KEYCIPHER_FLUSH_IN _IO(KEYCIPHER_MAGIC, 2)

/* helper: serialise one kernel_msg_t to a JSON object string */
static int msg_to_json(const kernel_msg_t *msg, char *buf, int buflen)
{
    int dlen = msg->len > 0 && msg->len < 256 ? msg->len : 0;
    return snprintf(buf, buflen,
        "{\"author\":\"%s\",\"timestamp\":%lld,\"data\":\"%.*s\",\"len\":%d}",
        msg->author, msg->tv_sec, dlen, msg->data, dlen
    );
}

/*
 * handle_get_stats - return JSON with buffer levels and peer statuses
 */
void handle_get_stats(int client_fd) {
    FILE *fp = fopen("/proc/keycipher/stats", "r");
    if (!fp) {
        dprintf(client_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
        return;
    }

    int incoming_used=0, incoming_free=0;
    int outgoing_used=0, outgoing_free=0;
    int chatroom_used=0, chatroom_free=0;
    int total_sent=0, total_received=0, total_blocked=0;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "incoming_used: %d", &incoming_used);
        sscanf(line, "incoming_free: %d", &incoming_free);
        sscanf(line, "outgoing_used: %d", &outgoing_used);
        sscanf(line, "outgoing_free: %d", &outgoing_free);
        sscanf(line, "chatroom_used: %d", &chatroom_used);
        sscanf(line, "chatroom_free: %d", &chatroom_free);
        sscanf(line, "total_sent: %d", &total_sent);
        sscanf(line, "total_received: %d", &total_received);
        sscanf(line, "total_blocked: %d", &total_blocked);
    }
    fclose(fp);

    /* build peers JSON */
    char peers_json[2048];
    peers_json[0] = '\0';
    strcat(peers_json, "[");
    peer_t *peers = peer_manager_get_all();
    int peer_count = peer_manager_count();
    for (int i = 0; i < peer_count; i++) {
        const char *status_str =
            (peers[i].status == PEER_CONNECTED) ? "connected" :
            (peers[i].status == PEER_BLOCKED)   ? "blocked"   : "offline";
        char entry[256];
        snprintf(entry, sizeof(entry),
            "{\"ip\":\"%s\",\"port\":%d,\"status\":\"%s\"}",
            peers[i].ip, peers[i].port, status_str);
        strcat(peers_json, entry);
        if (i < peer_count - 1) strcat(peers_json, ",");
    }
    strcat(peers_json, "]");

    char json[4096];
    snprintf(json, sizeof(json),
        "{"
        "\"incoming_used\":%d,\"incoming_free\":%d,"
        "\"outgoing_used\":%d,\"outgoing_free\":%d,"
        "\"total_sent\":%d,\"total_received\":%d,\"total_blocked\":%d,"
        "\"peers\":%s"
        "}",
        incoming_used, incoming_free,
        outgoing_used, outgoing_free,
        total_sent, total_received, total_blocked,
        peers_json
    );

    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        strlen(json), json);
}

/*
 * handle_get_messages - return JSON array of messages currently in inbox
 */
void handle_get_messages(int client_fd) {
    kernel_msg_t *msgs = direct_get_inbox();
    int count = direct_get_inbox_count();

    char json[8192];
    json[0] = '\0';
    strcat(json, "[");
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(json, ",");
        char entry[512];
        msg_to_json(&msgs[i], entry, sizeof(entry));
        strcat(json, entry);
    }
    strcat(json, "]");

    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        strlen(json), json);
}

/*
 * handle_get_outbox - return JSON array of outbox messages with waiting status
 */
void handle_get_outbox(int client_fd) {
    kernel_msg_t *msgs    = direct_get_outbox();
    int          *waiting = direct_get_outbox_waiting();
    int           count   = direct_get_outbox_count();

    char json[8192];
    json[0] = '\0';
    strcat(json, "[");
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(json, ",");
        int dlen = msgs[i].len > 0 && msgs[i].len < 256 ? msgs[i].len : 0;
        char entry[600];
        snprintf(entry, sizeof(entry),
            "{\"author\":\"%s\",\"timestamp\":%lld,\"data\":\"%.*s\",\"len\":%d,\"waiting\":%d}",
            msgs[i].author, msgs[i].tv_sec, dlen, msgs[i].data, dlen, waiting[i]);
        strcat(json, entry);
    }
    strcat(json, "]");

    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        strlen(json), json);
}

/*
 * handle_read_one - pop the front message from inbox_fifo, kernel decrypts it
 * returns the same message struct fields but with plaintext in data field
 */
void handle_read_one(int client_fd, const char *message_id) {
    (void)message_id; /* FIFO always pops front */

    int fd = open("/dev/keycipher_in", O_RDONLY);
    if (fd < 0) {
        perror("handle_read_one: open");
        dprintf(client_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
        return;
    }

    kernel_msg_t msg;
    int bytes = read(fd, &msg, sizeof(msg));
    close(fd);

    if (bytes != (int)sizeof(msg)) {
        dprintf(client_fd,
            "HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\n"
            "Content-Length: 29\r\n\r\n{\"error\":\"No messages waiting\"}");
        return;
    }

    /* remove from userspace inbox tracking */
    direct_pop_inbox_front();

    char json[512];
    msg_to_json(&msg, json, sizeof(json));

    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        strlen(json), json);
}

/*
 * handle_read_all - pop every message from inbox_fifo, kernel decrypts each
 */
void handle_read_all(int client_fd) {
    FILE *fp = fopen("/proc/keycipher/stats", "r");
    int count = 0;
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp))
            sscanf(line, "incoming_used: %d", &count);
        fclose(fp);
    }

    if (count == 0) {
        dprintf(client_fd,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n[]");
        return;
    }

    int fd = open("/dev/keycipher_in", O_RDONLY);
    if (fd < 0) {
        dprintf(client_fd, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
        return;
    }

    char json[8192];
    json[0] = '\0';
    strcat(json, "[");

    for (int i = 0; i < count; i++) {
        kernel_msg_t msg;
        int bytes = read(fd, &msg, sizeof(msg));
        if (bytes != (int)sizeof(msg)) break;
        direct_pop_inbox_front();
        if (i > 0) strcat(json, ",");
        char entry[512];
        msg_to_json(&msg, entry, sizeof(entry));
        strcat(json, entry);
    }

    close(fd);
    strcat(json, "]");

    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
        strlen(json), json);
}

/* stubs for chatroom — not used yet */
void handle_get_chatroom(int client_fd) {
    dprintf(client_fd,
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n[]");
}
void handle_send_direct(int client_fd, const char *body)   { (void)body; dprintf(client_fd, "HTTP/1.1 501 Not Implemented\r\n\r\n"); }
void handle_send_chatroom(int client_fd, const char *body) { (void)body; dprintf(client_fd, "HTTP/1.1 501 Not Implemented\r\n\r\n"); }
