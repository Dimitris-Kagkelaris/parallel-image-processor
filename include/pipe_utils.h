#ifndef PIPE_UTILS_H
#define PIPE_UTILS_H
// pipe helper functions
void create_pipe(int fd[2]);
void send_over_pipe(int fd, int data);
int receive_from_pipe(int fd, int *data);
void send_array_over_pipe(int fd, const void *data, size_t size);
void receive_array_from_pipe(int fd, void *data, size_t size);
#endif