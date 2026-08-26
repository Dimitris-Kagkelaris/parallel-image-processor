#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "util.h"

// helper functions
int max(int x, int y){
    return x < y ? y : x;
}

int min(int x, int y){
    return x > y ? y : x;
}

void *safe_malloc(size_t size){
    void *ptr = malloc(size);
    if (!ptr) {
        perror("malloc");
        exit(1);
    }
    return ptr;
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
    if (specs->width <= 0 || specs->height <= 0){
        fprintf(stderr, "[Dispatcher]: Invalid image dimensions\n");
        fclose(input);
        return 1;
    }

    fgetc(input);
    specs->header_size = (off_t)ftell(input);
    if (specs->maxval != 255) {
        fprintf(stderr, "Only 8-bit (maxval 255) PPMs are supported\n");
        fclose(input);
        return 1; 
    }

    fclose(input);
    return 0;
}