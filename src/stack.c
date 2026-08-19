#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "stack.h"
#include "util.h"

// stack functions
void initialize_stack(struct stack *s, int size){
    s->a = (int *) safe_malloc(size * sizeof(int));
    s->size = size;
    s->top = 0;
}

void free_stack(struct stack *s){
    free(s->a);
}


int get_size(struct stack *s){
    return s->top;
}

int get_top(struct stack *s){
    if(s->top == 0){
        printf("Stack is empty!\n");
        return -1;
    }

    return s->a[s->top-1];
}

bool is_empty(struct stack *s){
    return s->top == 0;
}

void push(struct stack *s, int elem){
    if(s->top >= s->size){
        printf("Stack is full!\n");
        return;
    }
    s->a[s->top] = elem;
    ++s->top;
}

void pop(struct stack *s){
    if(s->top == 0){
        printf("Cannot pop on an empty stack");
        return;
    }
    --s->top;
}