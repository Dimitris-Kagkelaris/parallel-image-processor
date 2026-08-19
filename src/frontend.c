#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#include "util.h"
#include "pipe_utils.h"

// typedef void (*command_handler)(void*);

struct command{
    const char* name;
    // pointer to function that takes arg void * and returns void
    void (*command_handler)(void*);
    const char* description;
};

void add_workers(void *arg);
void remove_workers(void *arg);
void print_process_info(void *arg);
void print_progress(void *arg);
void print_help(void *arg);
void clear_screen(void *arg);
void exit_application(void *arg);

const struct command commands[] = {
    {"add", add_workers, "Add workers"},
    {"remove", remove_workers, "Remove workers"},
    {"info", print_process_info, "Show process information"},
    {"progress", print_progress, "Show progress"},
    {"help", print_help, "Show help"},
    {"clear", clear_screen, "Clear screen"},
    {"exit", exit_application, "Exit the application"}
};

const unsigned int num_commands = sizeof(commands) / sizeof(command);

int frontend_out;
int worker_amount = 0;

void add_workers(void *arg){
    int number = *(int*)arg;
    int message = -1;
    receive_from_pipe(frontend_out, &message);
    if(message != 0){
        printf("Error encountered during adding workers\n");
        exit(1);
    }
    worker_amount += number;
    printf("Added %d workers!\n", number);
}

void remove_workers(void *arg){
    int number = *(int*)arg;
    int message = -1;
    receive_from_pipe(frontend_out, &message);
    if(message != 0){
        printf("Error encountered during removing workers\n");
        exit(1);
    }
    worker_amount -= number;
    printf("Removed %d workers!\n", number);
}

void print_process_info(void *arg){
    void **args = (void **)arg;
    pid_t frontend_pid = *(pid_t *)args[0];
    pid_t dispatcher_pid = *(pid_t *)args[1];
    printf("Frontend pid:   %d\n", frontend_pid);
    printf("Dispatcher pid: %d\n", dispatcher_pid);
    
    int worker_count;
    receive_from_pipe(frontend_out, &worker_count);
    printf("Workers: %d\n", worker_count);
    if (worker_count == 0) {
        break;
    }
    
    struct worker* workers = (struct worker*)safe_malloc(MAX_WORKERS * sizeof(struct worker));
    receive_array_from_pipe(frontend_out, workers, sizeof(workers));

    printf("  %-4s %-8s %-8s %-10s\n", "idx", "job", "pid", "completed");
    for (int i = 0; i < worker_count; ++i) {
        printf("  %-4d %-8d %-8d %-10d\n",
            i+1, workers[i].current_job, workers[i].pid, workers[i].jobs_completed);
    }
}

void print_progress(void *arg){
    int total_number_of_jobs = *(int*)arg;
    int jobs_count_done;
    receive_from_pipe(frontend_out, &jobs_count_done);
    printf("Progress: %.2f%%\n",
        (float) jobs_count_done/total_number_of_jobs * 100);
}

void print_help(void *arg){
    (void)arg;
    printf("Commands:\n");
    printf("[1] Add Workers <number>\n");
    printf("[2] Remove Workers <number>\n");
    printf("[3] Show Process Information\n");
    printf("[4] Show Progress\n");
    printf("[5] Help\n");
    printf("[6] Exit\n");
}

void clear_screen(void *arg){
    (void)arg;
    printf("\033[H\033[J");
    fflush(stdout);
}

void exit_application(void *arg){
    (void)arg;
    printf("Quitting...\n");
    exit(0);
}

void exit_handler(int signum){
    (void)signum;

    int status;
    wait(&status);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        const char msg[] = "\nProgress: 100%\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
        _exit(0);
    }

    const char msg[] = "\nDispatcher terminated with an error.\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
}

int main(int argc, char* argv[]){
    
    if(argc != 3){
        printf("Usage: %s <input> <output>\n", argv[0]);
        exit(1);
    }

    // set up signal communication with dispatcher
    struct sigaction sa;
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    sa.sa_handler = exit_handler;
    sa.sa_flags = 0;
    sa.sa_mask = sigset;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction (can't pair signal and handler)");
        exit(1);
    }
    
    pid_t frontend_pid = getpid();
    pid_t dispatcher_pid;

    // create 2 pipes for communication with dispatcher
    int fd[2];
    create_pipe(fd);
    int frontend_in = fd[1];
    int dispatcher_out = fd[0];
    int flags = fcntl(dispatcher_out, F_GETFL, 0);
    fcntl(dispatcher_out, F_SETFL, flags | O_NONBLOCK); // make the pipe non blocking
    
    create_pipe(fd);
    int dispatcher_in = fd[1];
    frontend_out = fd[0];
    
    pid_t p = fork();
    if(p<0){
        perror("fork");
        exit(1);
    }
    else if(p == 0){
        // kill dispatcher if frontend dies
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != frontend_pid) {
            exit(1);
        }

        // dispatcher closes frontend ends
        close(frontend_in);
        close(frontend_out);

        char arg0[100];
        char *dir = dirname(argv[0]);
        sprintf(arg0, "%s/dispatcher", dir);
        char arg1[10];
        char arg2[10];
        sprintf(arg1, "%d", dispatcher_out);
        sprintf(arg2, "%d", dispatcher_in);
        char *args[] = {
            arg0,
            argv[1],
            argv[2],
            arg1,
            arg2,
            NULL
        };
        execv(arg0, args);
        perror("execv for dispatcher");
        exit(1);
    }
    // frontend closes dispatcher ends
    close(dispatcher_in);
    close(dispatcher_out);
    dispatcher_pid = p;

    int total_number_of_jobs = 0;
    receive_from_pipe(frontend_out, &total_number_of_jobs);

    print_help();
    int code, num;
    while(1){
        code = -1; num = -1;
        printf("> ");
        int read = scanf("%d", &code);
        if(read == EOF){
            // reached EOF, wait for SIGCHLD
            while(1){ pause(); }
        }

        if(read != 1 || code < 1 || code > 6){
            printf("Not a valid command id: %d\n", code);
            flush_input_buffer();
            continue;
        }
        
        // if it's add or remove workers you need a number
        if(code < 3){
            if(scanf("%d", &num) != 1 || num <= 0){
                printf("Please provide a positive number of workers!\n");
                flush_input_buffer();
                continue;
            }

            if(code == 1 && num + worker_amount > MAX_WORKERS){
                if(worker_amount == MAX_WORKERS){
                    printf("Cannot add more workers. Already at max: %d.\n", MAX_WORKERS);
                    flush_input_buffer();
                    continue;
                }
                printf("Too many workers to add. I will set worker number to max: %d.\n", MAX_WORKERS);
                num = MAX_WORKERS - worker_amount;
            }
            else if(code == 2 && num > worker_amount){
                if(worker_amount == 0){
                    printf("Cannot remove workers. Already at 0.\n");
                    flush_input_buffer();
                    continue;
                }
                printf("Not enough workers to remove. I will set worker number to 0.\n");
                num = worker_amount;
            }
        }
        flush_input_buffer();

        if(code < 5){
            send_over_pipe(frontend_in, code);
            send_over_pipe(frontend_in, num);
            kill(dispatcher_pid, SIGUSR1);
        }

        switch(code){
            case 1:{
                add_workers(&num);
                break;
            }
            case 2:{
                remove_workers(&num);
                break;
            }
            case 3: {
                pid_t *pid_args[] = {&frontend_pid, &dispatcher_pid};
                print_process_info(pid_args);
                break;
            }
            case 4:{
                print_progress(&total_number_of_jobs);
                break;
            }
            case 5:{
                print_help();
                break;
            }
            case 6:{
                exit_application();
                break;
            }
            default:
                printf("Unreachable: invalid code: %d\n", code);
        };
    }
}