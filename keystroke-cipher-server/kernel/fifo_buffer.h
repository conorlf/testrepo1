#ifndef FIFO_BUFFER_H
#define FIFO_BUFFER_H

#include <linux/semaphore.h>
#include <linux/mutex.h>
#include "keycipher.h"



struct fifo_buffer {
    char messages[FIFO_SIZE][MSG_MAX_LEN];
    struct keycipher_message messages[FIFO_SIZE];
    int head;
    int tail;
    int count;
    struct semaphore slots_free;  /* blocks writers when 0 - backpressure */
    struct semaphore slots_used;  /* blocks readers when 0 */
    struct mutex lock;        /* protects head/tail/count */
};

void fifo_init(struct fifo_buffer *fifo);
int fifo_write(struct fifo_buffer *fifo, const struct keycipher_message *msg);
int fifo_read(struct fifo_buffer *fifo, struct keycipher_message *msg);
void fifo_flush(struct fifo_buffer *fifo);
int  fifo_count(struct fifo_buffer *fifo);

extern struct fifo_buffer inbox_fifo;
extern struct fifo_buffer outbox_fifo;
extern struct fifo_buffer chatroom_fifo;

#endif
