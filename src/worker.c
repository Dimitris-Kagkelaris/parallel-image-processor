#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <sys/prctl.h> 
#include <signal.h>
#include "util.h"
#include "pipe_utils.h"

#ifdef DEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

int main(int argc, char* argv[]){
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <pipe_out> <pipe_in> <input_fd> <output_fd>\n", argv[0]);
        return 1;
    }

    int pipe_in = atoi(argv[2]);
    int pipe_out = atoi(argv[1]);
    int input_file = atoi(argv[3]);
    int output_file = atoi(argv[4]);
    
    struct image_specs input_specs;
    struct image_specs output_specs;
    receive_array_from_pipe(pipe_out, &input_specs, sizeof(input_specs));
    receive_array_from_pipe(pipe_out, &output_specs, sizeof(output_specs));
    
    while(1){
        //read job from pipe
        int job_id;
        if (receive_from_pipe(pipe_out, &job_id) == 0) {
            // EOF, dispatcher has closed the pipe
            LOG("[Worker (%d)]: No more jobs. Exiting.\n", getpid());
            exit(0);
        }
        off_t read_byte_offset = input_specs.header_size + (off_t)job_id * PACKET_SIZE * STRIDE;
        ssize_t read_size = PACKET_SIZE * STRIDE;
        LOG("[Worker (%d)]: I will search editing from %ld byte offset in the input file.\n", getpid(), read_byte_offset);

        // read from file
        unsigned char buff[PACKET_SIZE * STRIDE];
        ssize_t bytes_read = 1;
        while(bytes_read > 0){// read until EOF
            bytes_read = pread(input_file, buff, min(read_size, sizeof(buff)), read_byte_offset);
            if (bytes_read == -1){ // error
                perror("read");
                exit(1);
            }

            ssize_t i;
            for(i = 0; i + STRIDE - 1 < bytes_read; i += STRIDE){
                unsigned char r = buff[i];
                unsigned char g = buff[i + 1];
                unsigned char b = buff[i + 2];
                unsigned char gray = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
                buff[i/STRIDE] = gray;
            }
            
            off_t write_byte_offset = (read_byte_offset - input_specs.header_size) / STRIDE + output_specs.header_size;
            ssize_t write_size = i / STRIDE;
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
        #ifdef SLEEP
        sleep(1);
        #endif
        LOG("[Worker (%d)]: I'm done with job %d.\n", getpid(), job_id);
        send_over_pipe(pipe_in, 0);
    }
}
