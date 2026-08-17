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

void flush_input_buffer(void);
int read_image_header(const char* image_name, struct image_specs *specs);
#endif