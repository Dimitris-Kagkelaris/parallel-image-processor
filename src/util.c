#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "util.h"

// helper functions
int max(int x, int y) {
    return x < y ? y : x;
}

int min(int x, int y) {
    return x > y ? y : x;
}

// stack functions
void initalize_stack(struct stack *s, int size){
    s->a = (int *) malloc(size * sizeof(int));
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

// pipe helper functions
void create_pipe(int fd[2]){
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }
}

void send_over_pipe(int fd, int data){
    if (write(fd, &data, sizeof(data)) != sizeof(data)) {
        perror("write to pipe");
        exit(1);
    }
}


int receive_from_pipe(int fd, int *data) {
    int bytes_read;
    
    while ((bytes_read = read(fd, data, sizeof(*data))) == -1 && errno == EINTR){
        // syscall was interrupted by a signal
        continue;
    }
    
    if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            // A non blocking pipe is empty (no error)
            return -1;
        }
        
        perror("read from pipe");
        exit(1);
    }
    
    return bytes_read;
}

void send_array_over_pipe(int fd, const void *data, size_t size) {
    size_t written = 0;
    const char *buf = (const char *) data;

    while (written < size) {
        ssize_t bytes_written = write(fd, buf + written, size - written);

        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue; // interrupted by signal, retry
            }
            perror("write to pipe");
            exit(1);
        }

        written += (size_t) bytes_written;
    }
}

void receive_array_from_pipe(int fd, void *data, size_t size) {
    size_t total_bytes_read = 0;
    char *buf = (char *) data;

    while (total_bytes_read < size) {
        ssize_t bytes_read = read(fd, buf + total_bytes_read, size - total_bytes_read);

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue; // interrupted by signal, retry
            }
            perror("read from pipe");
            exit(1);
        }
        if (bytes_read == 0) {
            fprintf(stderr, "unexpected EOF on pipe\n");
            exit(1);
        }

        total_bytes_read += (size_t) bytes_read;
    }
}

// string helper functions
void flush_input_buffer(void){
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// image helper functions

// reads a P6 PPM header
int read_image_header(const char* image_name, struct image_specs *specs){
    FILE *input = fopen(image_name, "r");
    if (!input){
        perror("fopen input"); 
        return 1;
    }


    // read 2 char magic number
    char magic[3];
    if (fscanf(input, "%2s", magic) != 1 || magic[0] != 'P' || magic[1] != '6') {
        fprintf(stderr, "[Dispatcher]: Not a P6 PPM file\n");
        fclose(input);
        return 1;
    }

    // skip whitespace/comments before width
    int c;
    while ((c = fgetc(input)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(input)) != EOF && c != '\n');
        }
        else if (c != ' ' && c != '\t' && c != '\n' && c != '\r'){
            ungetc(c, input);
            break;
        }
    }

    // read the specs and skip one char to get to the binary data
    if (fscanf(input, "%d %d %d", &specs->width, &specs->height, &specs->maxval) != 3) {
        fprintf(stderr, "[Dispatcher]: Malformed PPM header\n");
        fclose(input);
        return 1;
    }
    fgetc(input);
    
    specs->header_size = ftell(input);
    if (specs->maxval != 255) {
        fprintf(stderr, "Only 8-bit (maxval 255) PPMs are supported\n");
        fclose(input);
        return 1;
    }

    fclose(input);
    return 0;
}