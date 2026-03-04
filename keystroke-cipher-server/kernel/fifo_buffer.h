#ifndef FIFO_BUFFER_H
#define FIFO_BUFFER_H

#include <linux/semaphore.h>
#include <linux/mutex.h>
#include "keycipher.h"

struct fifo_buffer {
    char messages[FIFO_SIZE][MSG_MAX_LEN];
    int head;
    int tail;
    int count;
    struct semaphore slots_free;  /* blocks writers when 0 - backpressure */
    struct semaphore slots_used;  /* blocks readers when 0 */
    struct mutex lock;        /* protects head/tail/count */
};

void fifo_init(struct fifo_buffer *fifo);
int  fifo_write(struct fifo_buffer *fifo, const char *msg, size_t len);
int  fifo_read(struct fifo_buffer *fifo, char *msg, size_t len);
void fifo_flush(struct fifo_buffer *fifo);
int  fifo_count(struct fifo_buffer *fifo);

extern struct fifo_buffer incoming_fifo;
extern struct fifo_buffer outgoing_fifo;

#endif
