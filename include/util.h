#ifndef UTIL_H  
#define UTIL_H
#define MAX_WORKERS 500
// more workers will exceed the 1024 open file descriptor soft limit per process for the dispatcher and crash
#include <stdbool.h>

struct worker{
    int pid;
    int in;
    int out;
    int current_job;
    int jobs_completed;
    bool busy; // 0 means available 1 means busy
};

struct image_specs {
    int width;
    int height;
    int maxval; // maximum color value (should be 255)
    long long header_size;  // byte offset where pixel data starts
};

int min(int x, int y);
int max(int x, int y);

void create_pipe(int fd[2]);
void send_over_pipe(int fd, int data);
int receive_from_pipe(int fd, int *data);
void send_array_over_pipe(int fd, const void *data, size_t size);
void receive_array_from_pipe(int fd, void *data, size_t size);

struct stack {
    int *a;
    int size;
    int top;
};
void initalize_stack(struct stack *s, int size);
void free_stack(struct stack *s);
int get_size(struct stack *s);
int get_top(struct stack *s);
bool is_empty(struct stack *s);
void push(struct stack *s, int elem);
void pop(struct stack *s);

void flush_input_buffer(void);
int read_image_header(const char* image_name, struct image_specs *specs);
#endif