#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "pipe_utils.h"

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