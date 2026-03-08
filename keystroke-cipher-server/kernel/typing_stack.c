#include <linux/string.h>
#include "typing_stack.h"
/*  Simple LIFO stack used by the input interceptor that holds the characters in a temporary buffer until message is complete to add to fifo buffer
*   Implementation is minimal: no locking, single producer/consumer within one session.
*/
void typing_stack_init(struct typing_stack *stack)
{
    if (!stack){
        return;
    }
    memset(stack->data, 0, sizeof(stack->data));
    stack->top = 0;
}

int typing_stack_is_empty(const struct typing_stack *stack)
{
    if (!stack){
        return 1;
    }        
    return stack->top == 0;
}

int typing_stack_is_full(const struct typing_stack *stack)
{
    if (!stack){
        return 0;
    }
    return stack->top >= MSG_MAX_LEN;
}

int typing_stack_size(const struct typing_stack *stack)
{
    if (!stack){
        return 0;
    }
    return stack->top;
}

int typing_stack_push(struct typing_stack *stack, char ch)
{
    if (!stack){
        return -1;
    }
    if (stack->top >= MSG_MAX_LEN){
        return -1;
    }
    stack->data[stack->top++] = ch;
    return 0;
}

int typing_stack_pop(struct typing_stack *stack, char *ch)
{
    if (!stack || !ch){
        return -1;
    }
    if (stack->top == 0){
        return -1;
    }
    stack->top--;
    *ch = stack->data[stack->top];
    return 0;
}

int typing_stack_peek(const struct typing_stack *stack, char *ch)
{
    if (!stack || !ch){
        return -1;
    }
    if (stack->top == 0){
        return -1;
    }
    *ch = stack->data[stack->top - 1];
    return 0;
}

void typing_stack_clear(struct typing_stack *stack)
{
    if (!stack){
        return;
    }
    stack->top = 0;
}

