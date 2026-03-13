#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
        char *newline_char = strchr(line, '\n');
        if (newline_char) *newline_char = '\0';

        //skip if line is longer than ip max size
        if (strlen(line) >= sizeof(peers[peer_count].ip)) {
            fprintf(stderr, "IP too long, skipping: %s\n", line);
            continue;
        }

        //check MAX_PEERS isn't exceeded
        if (peer_count >= MAX_PEERS) {
            fprintf(stderr, "peer_manager_init: too many peers (max %d)\n", MAX_PEERS);
            break;
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

    peer_set_status("192.168.1.240", 1);

    for (int i=0; i < peer_count; i++) {
        printf("\t- IP READ IN: %s:%d (STATUS - %d)\n", peers[i].ip, peers[i].port, peers[i].status);
    }

    return 0;
}*/
/* END: TEST PLS REMOVE*/
