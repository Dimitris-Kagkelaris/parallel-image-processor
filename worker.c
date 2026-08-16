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


int main(int argc, char* argv[]){
    (void)argc;
    int pipe_in = atoi(argv[2]);
    int pipe_out = atoi(argv[1]);
    const int packet_size = atoi(argv[3]);
    char char_to_search = argv[4][0];
    int read_file = atoi(argv[5]);
    pid_t my_pid = getpid();
    int count;
    
    while(1){
        //read job from pipe
        int job_id, byte_offset;
        int bytes_read = receive_from_pipe(pipe_out, &job_id);
        if (bytes_read == 0) {
            // EOF, dispatcher has closed the pipe
            fprintf(stderr, "[Worker (%d)]: No more jobs. Exiting.\n", my_pid);
            exit(0);
        }
        byte_offset = job_id*packet_size;
        int work_size = packet_size;
        fprintf(stderr, "[Worker (%d)]: I will search starting from %d byte offset in the read file.\n", my_pid, byte_offset);
        count = 0;

        // read from file
        char buff[1024];
        ssize_t rcnt = 1;
        while(rcnt > 0){// read until EOF
            rcnt = pread(read_file, buff, min(work_size, sizeof(buff)), byte_offset);
            if (rcnt == -1){ // error
                perror("read");
                exit(1);
            }

            for(ssize_t i = 0; i < rcnt; ++i){
                if(buff[i] == char_to_search) 
                    ++count;
                usleep(30000);
            }
            work_size -= rcnt;
            byte_offset += rcnt;
        }
        fprintf(stderr, "[Worker (%d)]: I'm done.\n", my_pid);
        send_over_pipe(pipe_in, count);
    }
}