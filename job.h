#ifndef job_h
#define job_h

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "process.h"
#include "util.h"

/**
 *
 * Description of each job states:
 *
 * Pending: Initial state of all jobs.
 * Running: Job is running on foreground or background.
 * Stopped: Job is stopped by signal (ex. Ctrl+Z) or some errors.
 * Done:    Job is terminated.
 * Killed:  Job is killed by signal (ex. SIGKILL).
 *
**/
typedef enum jobstate
{
    Pending,
    Running,
    Stopped,
    Done,
    Killed
} JobState;

typedef enum jobmode
{
    FORE_MODE,
    BACK_MODE,
    BUILTIN_MODE
} JobMode;

typedef struct job
{
    int id;
    pid_t pgid;
    char *line;
    JobState job_state;
    JobMode job_mode;
    Process *process_queue; // the first one in linked list
    int running_procs;      // The total number of unfinished process. If this is reduced to 0, this job is "Done".
    struct job *next;
} Job;

typedef struct shell
{
    Job *jobs;
    Job *finished_jobs;
    Job *cur_job;
} Shell;

extern Shell *shell;

int set_jobid();
Job *new_job(size_t byte_size);
void insert_job(Job *new_job);
void delete_job(int job_id);
void insert_finished_job(Job *finished_job);
void free_jobs();

void run_job(Job *job);
void wait_fore_job(Job *job);
void wait_back_job();

void run_command(Process *command);
void jobs(char **args);
void fg(char **args);
void bg(char **args);

#endif
