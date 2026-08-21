#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/stat.h>
#include <libgen.h>
#include "util.h"
#include "stack.h"
#include "pipe_utils.h"

#ifdef DEBUG
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

int workers_count;
struct worker workers[MAX_WORKERS];

int input_file;
int output_file;
const int packet_size = 512;//////// welll see about that........................................
struct image_specs input_specs;
struct image_specs output_specs;

struct stack jobs;
int jobs_count_done;

pid_t dispatcher_pid;
char *dispatcher_dirname;
int dispatcher_in, dispatcher_out;
volatile sig_atomic_t command_arrived = 0; // flag to indicate that a command has arrived from frontend

void worker_init(int pos){
    int fd[2];

    create_pipe(fd);
    workers[pos].in = fd[1];
    int worker_out = fd[0];
    // make the fd close on exec so that the worker doesn't inherit it and cause a file descriptor leak
    fcntl(workers[pos].in, F_SETFD, FD_CLOEXEC);
    
    create_pipe(fd);
    int worker_in = fd[1];
    workers[pos].out = fd[0];
    int flags = fcntl(workers[pos].out, F_GETFL, 0);
    fcntl(workers[pos].out, F_SETFL, flags | O_NONBLOCK); // make the pipe non blocking
    fcntl(workers[pos].out, F_SETFD, FD_CLOEXEC); 
    
    workers[pos].busy = 0;
    workers[pos].current_job = -1;
    workers[pos].jobs_completed = 0;
    
    pid_t p = fork();
    if(p<0){
        perror("fork");
        exit(1);
    }
    else if(p == 0){
        // kill worker if dispatcher dies
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != dispatcher_pid) {
            exit(1);
        }
        
        char arg0[100];
        snprintf(arg0, sizeof(arg0), "%s/worker", dispatcher_dirname);
        char arg1[10];
        char arg2[10];
        char arg3[10];
        char arg4[10];
        char arg5[10];
        sprintf(arg1, "%d", worker_out);
        sprintf(arg2, "%d", worker_in);
        sprintf(arg3, "%d", packet_size);
        sprintf(arg4, "%d", input_file);
        sprintf(arg5, "%d", output_file);
        char *args[] = {
            arg0,
            arg1,
            arg2,
            arg3,
            arg4,
            arg5,
            NULL
        };
        execv(arg0, args);
        perror("execv for worker");
        exit(1);
    }
    // dispatcher closes worker ends
    close(worker_in);
    close(worker_out);
    workers[pos].pid = p;
    // send both image specs structs over the pipe to the worker
    send_array_over_pipe(workers[pos].in, &input_specs, sizeof(input_specs));
    send_array_over_pipe(workers[pos].in, &output_specs, sizeof(output_specs));
}

void add_workers(int num){
    for(int j = 0; j < num && j + workers_count < MAX_WORKERS; ++j){
        worker_init(j+workers_count);
    }
    workers_count = min(num + workers_count, MAX_WORKERS);
}

void remove_workers(int num){
    for(int j = max(0, workers_count - num); j < workers_count; j++){
        if (workers[j].busy) {
            push(&jobs, workers[j].current_job);
            LOG("[Dispatcher]: Pushed back job %d from worker %d.\n", workers[j].current_job, j);
        }

        int kill_id = workers[j].pid;
        close(workers[j].in);
        close(workers[j].out);
        kill(kill_id, SIGTERM);
        waitpid(kill_id, NULL, 0);
    }
    workers_count = max(0, workers_count - num);
}

void show_process_status(void){
    send_over_pipe(dispatcher_in, workers_count);
    if(workers_count == 0){
        return;
    }
    
    send_array_over_pipe(dispatcher_in, workers, sizeof(struct worker) * workers_count);
}

void show_progress(void){
    send_over_pipe(dispatcher_in, jobs_count_done);
}

void signal_handler(int signum){
    (void)signum;
    command_arrived = 1;
}

int main(int argc, char* argv[]){
    (void)argc;
    dispatcher_pid = getpid();
    
    dispatcher_dirname = dirname(argv[0]);

    // set up pipe with frontend
    dispatcher_out = atoi(argv[3]);
    dispatcher_in = atoi(argv[4]);
    
    // set up signal communication with frontend
    struct sigaction sa;
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    sa.sa_mask = sigset;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction (can't pair signal and handler)");
        exit(1);
    }
    
    // read input file
    if(read_image_header(argv[1], &input_specs) != 0){
        fprintf(stderr, "[Dispatcher]: Problem with input image");
        exit(1);
    }
    input_file = open(argv[1], O_RDONLY);
    if (input_file == -1){
        perror("Problem opening file to read");
        exit(1);
    }

    // check the file isn't corrupted
    struct stat st;
    if (fstat(input_file, &st) == -1) {
        perror("fstat");
        exit(1);
    }
    off_t expected_size = input_specs.header_size + (off_t)input_specs.width * input_specs.height * STRIDE;
    if (st.st_size != expected_size) {
        fprintf(stderr, "[Dispatcher]: Invalid/truncated PPM file\n");
        exit(1);
    }

    size_t work_size = (size_t)input_specs.width * (size_t)input_specs.height;
    if(work_size == 0){
        fprintf(stderr, "[Dispatcher]: Input image is empty\n");
        exit(1);
    }
    int number_of_jobs = (int)(work_size / packet_size + (work_size % packet_size > 0));
    LOG("[Dispatcher]: number of jobs: %d\n", number_of_jobs);
    // tell frontend how many jobs we have
    send_over_pipe(dispatcher_in, number_of_jobs);
    

    initialize_stack(&jobs, number_of_jobs+10);
    for(int i = 0; i < number_of_jobs; ++i){
        push(&jobs, i);
    }

    // initialize output file
    output_file = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_file == -1){
        perror("Problem opening file to write");
        exit(1);
    }
    char output_header[100];
    sprintf(output_header, "P5\n%d %d\n%d\n", input_specs.width, input_specs.height, input_specs.maxval);
    output_specs = input_specs;
    output_specs.header_size = (off_t)strlen(output_header);
    off_t output_file_size = output_specs.header_size + (off_t)output_specs.width * output_specs.height;
    if (ftruncate(output_file, output_file_size) == -1) {
        perror("ftruncate output file");
        close(output_file);
        exit(1);
    }
    ssize_t write_size = strlen(output_header);
    ssize_t total_bytes_written = 0;
    while(total_bytes_written < write_size){
        ssize_t bytes_written = write(output_file, output_header + total_bytes_written, 
            min(write_size - total_bytes_written, sizeof(output_header)));
        if (bytes_written == -1){
            perror("write");
            exit(1);
        }
        total_bytes_written += bytes_written;
    }

    while(1){
        if(jobs_count_done == number_of_jobs){
            fprintf(stderr, "[Dispatcher]: All jobs done. Exiting.\n");
            exit(0);
        }
        while(command_arrived){
            command_arrived = 0;

            int command_id = -1, amount = -1;
            while(receive_from_pipe(dispatcher_out, &command_id) > 0){
                switch(command_id){
                    case 0:
                        // busy waits until amount is received
                        while(receive_from_pipe(dispatcher_out, &amount) == -1);
                        add_workers(amount);
                        send_over_pipe(dispatcher_in, 0);
                        break;
                
                    case 1:
                        while(receive_from_pipe(dispatcher_out, &amount) == -1);
                        remove_workers(amount);
                        send_over_pipe(dispatcher_in, 0);
                        break;
                
                    case 2:
                        show_process_status();
                        break;
                
                    case 3:
                        show_progress();
                        break;
                
                    default:
                        printf("Unreachable: invalid command id: %d\n", command_id);
                };
            }
        }

        for(int i = 0; i < workers_count; ++i){
            int result;
            int bytes_read = receive_from_pipe(workers[i].out, &result);
            if(bytes_read == 0){ // workers[i].out has been closed or worker has died
                close(workers[i].in);
                close(workers[i].out);
                waitpid(workers[i].pid, NULL, 0); // collect the zombie
                fprintf(stderr, "[Dispatcher]: Detected dead worker with pid: %d\n", workers[i].pid);
                // if the worker was busy when it died, push back the job
                if (workers[i].busy) {
                    LOG("[Dispatcher]: Pushed back job %d.\n", workers[i].current_job);
                    push(&jobs, workers[i].current_job);
                }
                // restart the worker
                worker_init(i);
            }
            else if(bytes_read > 0){
                if(result != 0){
                    fprintf(stderr, "[Dispatcher]: Worker %d encountered an error while processing job %d\n", i, workers[i].current_job);
                    exit(1);
                }
                ++jobs_count_done;
                workers[i].busy = 0;
                ++workers[i].jobs_completed;
                LOG("[Dispatcher]: Collected job %d from worker %d\n", workers[i].current_job, i);
            }

            if (!workers[i].busy && !is_empty(&jobs)){
                int job_id = get_top(&jobs);
                // last check whether worker is dead
                int wait_response = waitpid(workers[i].pid, NULL, WNOHANG);
                if(wait_response == -1){
                    perror("Error in waitpid");
                }
                if(wait_response == 0){ // if there are available jobs and the child is still alive
                    LOG("[Dispatcher]: Gave job %d to worker %d.\n", job_id, i);
                    pop(&jobs);
                    send_over_pipe(workers[i].in, job_id);
                    workers[i].current_job = job_id;
                    workers[i].busy = 1;
                }
            }
        }
    }
}
