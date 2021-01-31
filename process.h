#ifndef process_h
#define process_h

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ARG_SIZE 8

typedef struct process
{
    pid_t pid;
    struct process *next;
    char *cmd;
    char *args[MAX_ARG_SIZE];
    char *read_filepath;
    char *write_filepath;
    int read_fd;
    int write_fd;
    int status;
} Process;

Process *new_process(Process *cur_process);

#endif
