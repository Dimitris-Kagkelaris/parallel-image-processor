#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include "util.h"
#include "pipe_utils.h"

struct command{
    const char* name;
    // pointer to function that takes arg void * and returns void
    void (*command_handler)(void*);
    const char* arguments;
    const char* description;
};

void add_workers(void *arg);
void remove_workers(void *arg);
void print_process_status(void *arg);
void print_progress(void *arg);
void print_help(void *arg);
void clear_screen(void *arg);
void exit_application(void *arg);

const struct command commands[] = {
    {"add", add_workers, "<count>",
        "Add workers, up to the maximum worker limit."},
    {"remove", remove_workers, "<count>",
        "Remove workers, stopping at zero."},
    {"status", print_process_status, NULL,
        "Show frontend, dispatcher, and worker information."},
    {"progress", print_progress, NULL,
        "Show the percentage of completed jobs."},
    {"help", print_help, NULL,
        "Show this command list."},
    {"clear", clear_screen, NULL,
        "Clear the terminal screen."},
    {"exit", exit_application, NULL,
        "Exit the application."}
};

const int num_commands = sizeof(commands) / sizeof(struct command);

pid_t frontend_pid;
pid_t dispatcher_pid;
int frontend_in;
int frontend_out;
int worker_amount = 0;
int total_number_of_jobs = 0;

bool validate_number_of_workers(char *number, int *number_of_workers){
    if(number == NULL){
        printf("Please provide a number in the range [1, %d]!\n", MAX_WORKERS);
        return false;
    }
    char *end;
    errno = 0;
    long num = strtol(number, &end, 10);
    if (end == number || *end != '\0' || errno == ERANGE || num <= 0){
        printf("Please provide a positive number of workers in the range [1, %d]!\n", MAX_WORKERS);
        return false;
    }

    if(num > MAX_WORKERS){
        // this is intentional so that it gets re clamped later
        num = MAX_WORKERS + 10;
    }
    *number_of_workers = num;
    return true;
}

void add_workers(void *arg){
    if(worker_amount == MAX_WORKERS){
        printf("Cannot add more workers. Already at max: %d.\n", MAX_WORKERS);
        return;
    }
    int number;
    if(!validate_number_of_workers(arg, &number)){
        return;
    }
    if(number + worker_amount > MAX_WORKERS){
        printf("Too many workers to add. I will set worker number to max: %d.\n", MAX_WORKERS);
        number = MAX_WORKERS - worker_amount;
    }

    // send command id, number + notify dispatcher
    send_over_pipe(frontend_in, 0);
    send_over_pipe(frontend_in, number);
    kill(dispatcher_pid, SIGUSR1);

    // response from dispatcher
    int message = -1;
    if(receive_from_pipe(frontend_out, &message) == 0){
        printf("Dispatcher has finished — Workers cannot be added.\n");
        return;
    }
    if(message != 0){
        printf("Error encountered during adding workers\n");
        exit(1);
    }
    worker_amount += number;
    printf("Added %d workers!\n", number);
}

void remove_workers(void *arg){
    if(worker_amount == 0){
        printf("Cannot remove workers. Already at min: 0.\n");
        return;
    }
    int number;
    if(!validate_number_of_workers(arg, &number)){
        return;
    }
    if(number > worker_amount){
        printf("Not enough workers to remove. I will set worker number to 0.\n");
        number = worker_amount;
    }

    // send command id, number + notify dispatcher
    send_over_pipe(frontend_in, 1);
    send_over_pipe(frontend_in, number);
    kill(dispatcher_pid, SIGUSR1);

    // response from dispatcher
    int message = -1;
    if(receive_from_pipe(frontend_out, &message) == 0){
        printf("Dispatcher has finished — Workers cannot be removed.\n");
        return;
    }
    if(message != 0){
        printf("Error encountered during removing workers\n");
        exit(1);
    }
    worker_amount -= number;
    printf("Removed %d workers!\n", number);
}

void print_process_status(void *arg){
    // send command id + notify dispatcher
    send_over_pipe(frontend_in, 2);
    kill(dispatcher_pid, SIGUSR1);

    (void)arg;
    printf("Frontend pid:   %d\n", frontend_pid);
    printf("Dispatcher pid: %d\n", dispatcher_pid);
    
    int worker_count;
    if(receive_from_pipe(frontend_out, &worker_count) == 0){
        printf("Dispatcher has finished — no process status to report.\n");
        return;
    }
    printf("Workers: %d\n", worker_count);
    if (worker_count == 0) {
        return;
    }
    
    struct worker* workers = (struct worker*)safe_malloc(worker_count * sizeof(struct worker));
    receive_array_from_pipe(frontend_out, workers, worker_count * sizeof(struct worker));

    printf("  %-4s %-8s %-8s %-10s\n", "idx", "job", "pid", "completed");
    for (int i = 0; i < worker_count; ++i) {
        printf("  %-4d %-8d %-8d %-10d\n",
            i+1, workers[i].current_job, workers[i].pid, workers[i].jobs_completed);
    }
    free(workers);
}

void print_progress(void *arg){
    // send command id + notify dispatcher
    send_over_pipe(frontend_in, 3);
    kill(dispatcher_pid, SIGUSR1);
    (void)arg;
    int jobs_count_done;
    if(receive_from_pipe(frontend_out, &jobs_count_done) == 0){
        printf("Dispatcher has finished — no live progress to report.\n");
        return;
    }
    printf("Progress: %.2f%%\n",
        (float) jobs_count_done/total_number_of_jobs * 100);
}

void print_help(void *arg){
    (void)arg;
    printf("Commands:\n");
    for(int i = 0; i < num_commands; ++i){
        if(commands[i].arguments == NULL){
            printf("  %s: %s\n",
                commands[i].name,
                commands[i].description);
            }
        else{
            printf("  %s %s: %s\n",
                commands[i].name,
                commands[i].arguments,
                commands[i].description);
        }
    }
}

void clear_screen(void *arg){
    (void)arg;
    printf("\033[H\033[J");
}

void exit_application(void *arg){
    (void)arg;
    printf("Exiting...\n");
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

    if(strcmp(argv[1], argv[2]) == 0){
        printf("Input and output files must be different!\n");
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
    
    frontend_pid = getpid();

    // create 2 pipes for communication with dispatcher
    int fd[2];
    create_pipe(fd);
    frontend_in = fd[1];
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
        snprintf(arg0, sizeof(arg0), "%s/dispatcher", dir);
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

    if(receive_from_pipe(frontend_out, &total_number_of_jobs) == 0){
        printf("Dispatcher failed to start!\n")
        exit(1);
    }

    print_help(NULL);
    char *line = NULL;
    size_t buffer_len = 0;
    while(1){
        printf("> ");
        fflush(stdout);
        
        if(getline(&line, &buffer_len, stdin) == -1){
            putchar('\n');
            exit_application(NULL);
        }
        char *cmd = strtok(line, " \t\n");
        char *cmd_args = strtok(NULL, " \t\n");
        if(cmd == NULL){
            continue;
        }

        // dispatch
        bool invalid_command = 1;
        for(int i = 0; i < num_commands; ++i){
            if(strcmp(cmd, commands[i].name) == 0){
                invalid_command = 0;
                commands[i].command_handler(cmd_args);
                break;
            }
        }
        if(invalid_command){
            printf("Invalid command!\n");
        }
    }
}
