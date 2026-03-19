#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include "peer_manager.h"

#define MAX_LINE 256 //can be adjusted if too small

static peer_t peers[MAX_PEERS];
static int peer_count = 0;

/*open config_file (peers.conf)
  parse each non-comment line as an IP address
  populate peers[] with ip, default port 8443, PEER_OFFLINE status
  return number of peers loaded */
int peer_manager_init(const char *config_file_name) {
    char buf[MAX_LINE];
    peer_count = 0; //clear just in case re-init

    FILE* fptr = fopen(config_file_name, "r");
    if (!fptr) return -1;

    //parse file
    while ((fgets(buf, sizeof buf, fptr)) != NULL) {
        char *line = buf;
        //trim leading whitespace
        while (*line == ' ' || *line == '\t') line++;
        //skip blank lines and comments
        if (*line == '\0' || *line == '\n' || *line == '#')
            continue;

        //remove trailing newline (changes '\n' in str to '\0' terminator)
        char *newline_char = strchr(line, '\n'); //LINUX
        //char *newline_char = strpbrk(line, "\r\n"); /*REMOVEEEEE - WINDOWS*/
        if (newline_char) *newline_char = '\0';

        //check MAX_PEERS isn't exceeded
        if (peer_count >= MAX_PEERS) {
            fprintf(stderr, "peer_manager_init: too many peers (max %d)\n", MAX_PEERS);
            break;
        }

        //skip if line is longer than ip max size
        if (strlen(line) >= sizeof(peers[peer_count].ip)) {
            fprintf(stderr, "IP too long, skipping: %s\n", line);
            continue;
        }

        //store into peers[]
        strcpy(peers[peer_count].ip, line);
        peers[peer_count].port = 8443;
        peers[peer_count].status = PEER_OFFLINE;
        peers[peer_count].socket_fd = -1; //initialised to closed

        peer_count++; //track number of IPs, for adding to peers array
    }
    fclose(fptr);
    return peer_count;
}

/*set *count = peer_count
  return peers[] array pointer
  used by handlers.c for GET /api/peers */
peer_t *peer_get_all(int *count){
    *count = peer_count; //updates count
    return peers; //returns peer list
}

/*loops through all loaded peers
attempt a TCP connection to each
update socket_fd and status accordingly
return how many succeeded*/
int peer_manager_connect_all() {
    int sockets_connected = 0;

    //for each peer in peers
    for (int i=0; i < peer_count; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0); //new tcp connection, AF_INET is just IPv4
        if (sock < 0) {
            perror("socket");
            peers[i].status = PEER_OFFLINE;
            continue;
        }

        //creates zeroed out address to connect to
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(peers[i].port); //sets port number + endianness to host order

        //converts to binary (presentable to network format) and checks IP is valid
        if (inet_pton(AF_INET, peers[i].ip, &addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP: %s\n", peers[i].ip);
            close(sock);
            peers[i].status = PEER_OFFLINE;
            continue; //skips if not return 1
        }

        printf("Connecting to %s...\n", peers[i].ip); /*DEBUG - CHECK IF TIMING OUT*/
        //smaller timeout, in case fail to connect in code below
        struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        //attempt connection
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            //if success
            peers[i].socket_fd = sock;
            peers[i].status = PEER_CONNECTED;
            sockets_connected++;
            printf("Connected to %s:%d\n", peers[i].ip, peers[i].port);
        } else {
            //if fail
            perror("connect");
            close(sock);
            peers[i].socket_fd = -1;
            peers[i].status = PEER_OFFLINE;
            printf("Failed to connect to %s:%d\n", peers[i].ip, peers[i].port);
        }
    }

    return sockets_connected; // connection worked
}

/* find peer in peers[] by IP string match
    update peer->status
    PEER_BLOCKED causes frontend to show spinner on that peer
    PEER_CONNECTED resumes normal send flow */
void peer_set_status(const char *ip, peer_status_t status){
    if (!ip) return;
    for (int i=0; i < peer_count; i++) {
        if (strcmp(peers[i].ip, ip) == 0) {
            //if matching IPs
            peers[i].status = status;
            printf("IP CHANGE IN: %s:%d (STATUS - %d)\n", peers[i].ip, peers[i].port, peers[i].status);
            break;
        }
    }
}

/* close all open socket_fd connections
    reset peer_count to 0 */
void peer_manager_cleanup(void){
    for (int i=0; i < peer_count; i++) {
        //check for valid fd -> 0, 1, 2 are std_in, out, err, so fd must be >= 3
        if (peers[i].socket_fd > 2) {
            close(peers[i].socket_fd);
            peers[i].socket_fd = -1;   //mark as closed
        }

    }
    peer_count = 0;
}

/* TEST PLS REMOVE*/
/*int main(void) {
    printf("Read IP count: %d\n", peer_manager_init("../../peers.conf"));

    for (int i=0; i < peer_count; i++) {
        printf("\t- IP READ IN: %s:%d (STATUS - %d)\n", peers[i].ip, peers[i].port, peers[i].status);
    }
    peer_manager_connect_all();

    peer_set_status("192.168.1.240", 1);

    for (int i=0; i < peer_count; i++) {
        printf("\t- IP READ IN: %s:%d (STATUS - %d)\n", peers[i].ip, peers[i].port, peers[i].status);
    }

    return 0;
}*/
/* END: TEST PLS REMOVE*/
