#ifndef STACK_H
#define STACK_H
#include <stdbool.h>

// stack implementation
struct stack {
    int *a;
    int size;
    int top;
};
void initialize_stack(struct stack *s, int size);
void free_stack(struct stack *s);
int get_size(struct stack *s);
int get_top(struct stack *s);
bool is_empty(struct stack *s);
void push(struct stack *s, int elem);
void pop(struct stack *s);
#endif