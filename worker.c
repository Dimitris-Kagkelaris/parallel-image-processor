#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include "util.h"
#include <sys/prctl.h>
#include <signal.h>
#define STRIDE 3


int main(int argc, char* argv[]){
    (void)argc;
    int pipe_in = atoi(argv[2]);
    int pipe_out = atoi(argv[1]);
    const int packet_size = atoi(argv[3]);
    int input_file = atoi(argv[4]);
    int output_file = atoi(argv[5]);
    
    struct image_specs specs;
    receive_array_from_pipe(pipe_out, &specs, sizeof(specs));
    
    pid_t my_pid = getpid();
    
    while(1){
        //read job from pipe
        int job_id;
        if (receive_from_pipe(pipe_out, &job_id) == 0) {
            // EOF, dispatcher has closed the pipe
            fprintf(stderr, "[Worker (%d)]: No more jobs. Exiting.\n", my_pid);
            exit(0);
        }
        int read_byte_offset = specs.header_size + job_id * packet_size * STRIDE;
        int read_size = packet_size * STRIDE;
        fprintf(stderr, "[Worker (%d)]: I will search editing from %d byte offset in the input file.\n", my_pid, read_byte_offset);

        // read from file
        unsigned char buff[1024];
        ssize_t bytes_read = 1;
        while(bytes_read > 0){// read until EOF
            bytes_read = pread(input_file, buff, min(read_size, sizeof(buff)), read_byte_offset);
            if (bytes_read == -1){ // error
                perror("read");
                exit(1);
            }

            ssize_t i;
            for(i = 0; i + STRIDE - 1 < bytes_read && i < 1021; i += STRIDE){
                unsigned char r = buff[i];
                unsigned char g = buff[i + 1];
                unsigned char b = buff[i + 2];
                unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
                buff[i/STRIDE] = gray;
                // usleep(30000);
            }
            
            int write_byte_offset = (read_byte_offset - specs.header_size) / STRIDE + specs.header_size;
            ssize_t write_size = i/STRIDE;
            ssize_t total_bytes_written = 0;
            while(total_bytes_written < write_size){
                ssize_t bytes_written = pwrite(output_file, buff + total_bytes_written, 
                    min(write_size - total_bytes_written, sizeof(buff)), write_byte_offset);
                if (bytes_written == -1){
                    perror("write");
                    exit(1);
                }
                total_bytes_written += bytes_written;
                write_byte_offset += bytes_written;
            }
            
            read_size -= i;
            read_byte_offset += i;
        }
        fprintf(stderr, "[Worker (%d)]: I'm done with job %d.\n", my_pid, job_id);
        send_over_pipe(pipe_in, 0);
    }
}