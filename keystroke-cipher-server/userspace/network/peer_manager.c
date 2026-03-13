#include <stdio.h>
#include <string.h>
#include "peer_manager.h"

#define MAX_LINE 256
//can be adjusted if too small

static peer_t peers[MAX_PEERS];
static int peer_count = 0;

/* description of peer_manager_init func:
- open config_file (peers.conf)
- parse each non-comment line as an IP address
- populate peers[] with ip, default port 8443, PEER_OFFLINE status
- return number of peers loaded */
int peer_manager_init(const char *config_file_name) {
    char buf[MAX_LINE];
    int count = 0;
    peer_count = 0; //clear just in case re-init

    FILE* fptr = fopen(config_file_name, "r");
    if (!fptr) return 1;

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

        //store into peers[]
        strcpy(peers[count].ip, line);
        peers[count].port = 8443;
        peers[count].status = PEER_OFFLINE;

        count++; //track number of IPs, for adding to peers array
        peer_count++;
    }
    fclose(fptr);
    return count;
}

peer_t *peer_get_all(int *count)
{
    /* set *count = peer_count
       return peers[] array pointer
       used by handlers.c for GET /api/peers */
}

void peer_set_status(const char *ip, peer_status_t status)
{
    /* find peer in peers[] by IP string match
       update peer->status
       PEER_BLOCKED causes frontend to show spinner on that peer
       PEER_CONNECTED resumes normal send flow */
}

void peer_manager_cleanup(void)
{
    /* close all open socket_fd connections
       reset peer_count to 0 */
}

/* TEST PLS REMOVE*/
int main(void) {
    printf("Read IP count: %d\n", peer_manager_init("../../peers.conf"));

    for (int i=0; i < peer_count; i++) {
        printf("\t- IP READ IN: %s:%d (STATUS - %d)\n", peers[i].ip, peers[i].port, peers[i].status);
    }

    return 0;
}
/* END: TEST PLS REMOVE*/
