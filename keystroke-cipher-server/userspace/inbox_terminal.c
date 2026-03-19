/*
 * inbox_terminal.c — interactive inbox for KeyCipher
 *
 * Shows how many encrypted messages are waiting in the inbox FIFO.
 * Press Enter to pop the front message — kernel decrypts it — display plaintext.
 *
 * Build:
 *   gcc -Wall -o inbox_terminal inbox_terminal.c
 *
 * Run (as root, after kernel module is loaded):
 *   sudo ./inbox_terminal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_IN    "/dev/keycipher_in"
#define PROC_STATS   "/proc/keycipher/stats"

/* Must match kernel/keycipher.h struct keycipher_message layout */
typedef struct {
    long long tv_sec;
    long long tv_nsec;
    char      author[64];
    char      data[256];
    int       len;
} inbox_msg_t;

/* Read incoming_used count from /proc/keycipher/stats */
static int get_inbox_count(void)
{
    FILE *f = fopen(PROC_STATS, "r");
    if (!f) return -1;

    char line[128];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "incoming_used: %d", &count) == 1)
            break;
    }
    fclose(f);
    return count;
}

int main(void)
{
    int dev_fd = open(DEVICE_IN, O_RDONLY);
    if (dev_fd < 0) {
        perror("inbox_terminal: failed to open " DEVICE_IN);
        fprintf(stderr, "Is the kernel module loaded? Run: sudo insmod keycipher.ko\n");
        return 1;
    }

    printf("┌─────────────────────────────────────┐\n");
    printf("│        KeyCipher — Inbox            │\n");
    printf("│  Press Enter to decrypt next msg    │\n");
    printf("│  Ctrl-C to exit                     │\n");
    printf("└─────────────────────────────────────┘\n\n");

    for (;;) {
        int waiting = get_inbox_count();
        if (waiting < 0)
            printf("[inbox] %d message(s) waiting  — press Enter to decrypt next\n", 0);
        else
            printf("[inbox] %d message(s) waiting  — press Enter to decrypt next\n", waiting);

        /* Wait for user to press Enter */
        int c;
        while ((c = getchar()) != '\n' && c != EOF);

        if (waiting == 0) {
            printf("[inbox] No messages waiting.\n\n");
            continue;
        }

        /* Pop front message — kernel decrypts on read */
        inbox_msg_t msg;
        int bytes = read(dev_fd, &msg, sizeof(msg));
        if (bytes < 0) {
            perror("[inbox] read failed");
            break;
        }
        if (bytes < (int)sizeof(msg)) {
            printf("[inbox] short read (%d bytes), skipping\n\n", bytes);
            continue;
        }

        int dlen = msg.len > 0 && msg.len < 256 ? msg.len : 0;

        printf("\n┌─── Message ───────────────────────────\n");
        printf("│ From      : %.64s\n", msg.author[0] ? msg.author : "unknown");
        printf("│ Received  : %lld\n",  msg.tv_sec);
        printf("│ Plaintext : %.*s\n",  dlen, msg.data);
        printf("└───────────────────────────────────────\n\n");
    }

    close(dev_fd);
    return 0;
}
