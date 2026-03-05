#include "fifo_buffer.h"

struct fifo_buffer incoming_fifo;
struct fifo_buffer outgoing_fifo;

void fifo_init(struct fifo_buffer *fifo)
{
    /* set head, tail, count to 0*/
    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;

    sema_init(&fifo->slots_free, FIFO_SIZE); // all slots available
    sema_init(&fifo->slots_used, 0); // nothing to read yet
    
    mutex_init(&fifo->lock); // mutex_init lock 
}

int fifo_write(struct fifo_buffer *fifo, const char *msg, size_t len)
{
    if (down_interruptible(&fifo->slots_free))
        return -ERESTARTSYS;   // interrupted by signal

    mutex_lock(&fifo->lock); //mutex_lock

    strncpy(fifo->messages[fifo->tail], msg, MSG_MAX_LEN); // copy msg into messages[tail]
    fifo->messages[fifo->tail][MSG_MAX_LEN - 1] = '\0'; // safety

    fifo->tail = (fifo->tail + 1) % FIFO_SIZE; //advance tail with wraparound: tail = (tail + 1) % FIFO_SIZE
    fifo->count++; 

    mutex_unlock(&fifo->lock); //mutex unlock

    up(&fifo->slots_used);  // wake up slots_used to wake any blocked reader

    return 0;
}


int fifo_read(struct fifo_buffer *fifo, char *msg, size_t len)
{
    /* down_interruptible slots_used
         blocks here if buffer is empty
         returns -ERESTARTSYS if interrupted*/
    if (down_interruptible(&fifo->slots_used)) {
        return -ERESTARTSYS;
    }

    mutex_lock(&fifo->lock); //mutex_lock
    strncpy(msg, fifo->messages[fifo->head], len); msg[len - 1] = '\0'; // copy messages[head] into msg
    
    fifo->head = (fifo->head + 1) % FIFO_SIZE; // advance head with wraparound: head = (head + 1) % FIFO_SIZE
    fifo->count--;// decrement count
    
    mutex_unlock(&fifo->lock); // mutex_unlock
    up(&fifo->slots_free); // up slots_free - wakes any blocked writer (unblocks a peer) 

    return 0;
}

void fifo_flush(struct fifo_buffer *fifo)
{
    mutex_lock(&fifo->lock); // mutex_lock

    // reset head, tail, count to 0
    fifo->head = 0;
    fifo->tail = 0;
    fifo->count = 0;

    // re-initialise semaphores
    sema_init(&fifo->slots_free, FIFO_SIZE);
    sema_init(&fifo->slots_used, 0);

    mutex_unlock(&fifo->unlock); // mutex_unlock
}

int fifo_count(struct fifo_buffer *fifo)
{
    return fifo->count;
}
