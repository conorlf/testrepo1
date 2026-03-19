#ifndef KEYCIPHER_H
#define KEYCIPHER_H
#include <linux/ioctl.h>
#include <linux/time64.h>
#define DEVICE_NAME   "keycipher"
#define FIFO_SIZE    64
#define MSG_MAX_LEN  256
#define AUTHOR_MAX   64
#define KEYCIPHER_MAGIC          'K'
#define KEYCIPHER_SET_MODE_READ  _IO(KEYCIPHER_MAGIC, 0)
#define KEYCIPHER_SET_MODE_WRITE _IO(KEYCIPHER_MAGIC, 1)
#define KEYCIPHER_FLUSH_IN       _IO(KEYCIPHER_MAGIC, 2)
#define KEYCIPHER_GET_STATS      _IOR(KEYCIPHER_MAGIC, 3, struct keycipher_stats)
#define MODE_READ  0
#define MODE_WRITE 1

/* Outbox message: timestamp when Enter was pressed, author, and encrypted payload */
struct keycipher_message {
    struct timespec64 timestamp;
    char              author[AUTHOR_MAX];
    char              data[MSG_MAX_LEN];
    int               len;
};

struct keycipher_stats {
    int  incoming_used;
    int  incoming_free;
    int  outgoing_used;
    int  outgoing_free;
    long total_sent;
    long total_received;
    long total_decrypted;
    int chatroom_used;
    int chatroom_free;
};

#endif
