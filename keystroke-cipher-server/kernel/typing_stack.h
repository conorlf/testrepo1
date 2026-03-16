#ifndef TYPING_STACK_H
#define TYPING_STACK_H

#include "keycipher.h"  /* for MSG_MAX_LEN */


struct typing_stack {
    char data[MSG_MAX_LEN];
    int  top;  /* index of next free slot, 0 when empty */
};
void typing_stack_init(struct typing_stack *stack);
int typing_stack_push(struct typing_stack *stack, char ch);
int typing_stack_pop(struct typing_stack *stack, char *ch);
int typing_stack_peek(const struct typing_stack *stack, char *ch);
void typing_stack_clear(struct typing_stack *stack);
int typing_stack_is_empty(const struct typing_stack *stack);
int typing_stack_is_full(const struct typing_stack *stack);
int typing_stack_size(const struct typing_stack *stack);
int typing_stack_drain(struct typing_stack *stack, char *buf, int max_len);// this is used for sending messages to the FIFO buffer from the LIFO stack

#endif /* TYPING_STACK_H */